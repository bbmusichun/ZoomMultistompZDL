#!/usr/bin/env python3
"""Build BBDelay.ZDL.

Default: UI-only smoke binary (audio_nop=True), for early loader/UI checks
only. It is NOT an audible delay.

After G1Xon ZDL loading and ctx[3] memory have been verified, create the DSP
candidate with:

    BBDELAY_ENABLE_DSP=1 python3 src/custom/bbdelay/build.py

BB Delay is intentionally excluded from build_all.py until G1Xon hardware
validation is complete.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "build"))

from linker import LinkerConfig, link, params_from_manifest  # noqa: E402


def find_ti_root() -> Path:
    candidates: list[Path] = []
    override = os.environ.get("TI_CGT_C6000")
    if override:
        candidates.append(Path(override))
    candidates.extend([
        Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS"),
        Path("/Applications/ti/ccs2050/ccs/tools/compiler/ti-cgt-c6000_8.5.0.LTS"),
    ])
    for candidate in candidates:
        if (candidate / "bin" / "cl6x").is_file():
            return candidate
    raise FileNotFoundError(
        "TI C6000 compiler not found. Set TI_CGT_C6000 to the compiler root "
        "(the directory containing bin/cl6x)."
    )


def main() -> None:
    manifest = json.loads((HERE / "manifest_pedal.json").read_text(encoding="utf-8"))
    output_basename = manifest.get("output_basename", manifest["effect_name"])
    if len(output_basename) > 8:
        raise ValueError("Zoom ZDL output basename must be 8 characters or fewer")

    # Refuse to reuse an effect ID that is already present in the checked-in DB.
    db_path = ROOT / "tools" / "effects_db.json"
    if db_path.exists():
        db = json.loads(db_path.read_text(encoding="utf-8"))
        for group in ("custom", "stock", "matujuice"):
            for item in db.get(group, []):
                if item.get("gid") == manifest["gid"] and item.get("fxid") == manifest["fxid"]:
                    raise ValueError(
                        f"Effect ID collision: gid={manifest['gid']} fxid={manifest['fxid']} "
                        f"is already used by {item.get('name')!r}"
                    )

    ti_root = find_ti_root()
    cl6x = ti_root / "bin" / "cl6x"
    source = HERE / "bbdelay.c"
    obj = HERE / "bbdelay.obj"
    out_dir = ROOT / "dist"
    out_dir.mkdir(exist_ok=True)
    output = out_dir / f"{output_basename}.ZDL"

    cflags = [
        "--c99",
        "--opt_level=2",
        "-mv6740",
        "--abi=eabi",
        "--mem_model:data=far",
        f"--include_path={ti_root / 'include'}",
    ]

    print(f"[BBDelay] compiling {source.name} -> {obj.name}")
    subprocess.run(
        [str(cl6x), *cflags, "-c", str(source), f"--output_file={obj}"],
        check=True,
        cwd=HERE,
    )

    for junk in ("compiler.opt", "linker.cmd"):
        path = HERE / junk
        if path.exists():
            path.unlink()

    enable_dsp = os.environ.get("BBDELAY_ENABLE_DSP") == "1"
    if enable_dsp:
        print("[BBDelay] DSP ENABLED: candidate build only; G1Xon hardware proof is still required.")
    else:
        print("[BBDelay] UI-ONLY SMOKE BUILD: audio is replaced with NOP_RETURN.")

    cfg = LinkerConfig(
        effect_name=manifest["effect_name"],
        audio_func_name=manifest["audio_func_name"],
        gid=manifest["gid"],
        fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj,
        output_path=output,
        fxid_version=manifest.get("fxid_version", "1.00").encode("ascii"),
        flags_byte=manifest.get("flags_byte", 0x01),
        audio_nop=not enable_dsp,
        # Keep the custom _init path off for the unverified G1Xon target.
        materialize_init=False,
        # First two controls use the stock LineSel pair. Only the DSP
        # candidate enables compact synthesized handlers for controls 3-9;
        # this path is not yet hardware-proven on G1Xon.
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=enable_dsp,
        synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__",
        knob_positions=[(2, 14, 46), (3, 55, 46), (4, 96, 46)],
        dsp_cost=27.0,
    )
    link(cfg)
    print(f"[BBDelay] done -> {output}")
    print("[BBDelay] no hardware installation or G1Xon audio validation was performed.")


if __name__ == "__main__":
    main()
