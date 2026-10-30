#!/usr/bin/env python3
"""Build BBAuto.ZDL. Use build_all.py bbauto to stage it as a probe."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))

from linker import LinkerConfig, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402
from custom_covers import make_cover  # noqa: E402

TI_ROOT = Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS")
CL6X = TI_ROOT / "bin" / "cl6x"

CFLAGS = [
    "--c99", "--opt_level=2", "-mv6740", "--abi=eabi",
    "--mem_model:data=far", f"--include_path={TI_ROOT}/include",
]


def main() -> None:
    manifest = json.loads((HERE / "manifest_pedal.json").read_text())
    write_param_header(manifest, HERE / "bbauto_params.h", "BBAUTO")

    obj = HERE / "bbauto.obj"
    out_dir = ROOT / "dist"
    out_dir.mkdir(exist_ok=True)
    out_zdl = out_dir / "BBAuto.ZDL"

    subprocess.run(
        [str(CL6X), *CFLAGS, "--define=BBAUTO_AUDIO_FUNC=Fx_DLY_BBAuto",
         "-c", str(HERE / "bbauto.c"), f"--output_file={obj}"],
        cwd=HERE, check=True,
    )
    for name in ("compiler.opt", "linker.cmd"):
        scratch = HERE / name
        if scratch.exists():
            scratch.unlink()

    link(LinkerConfig(
        materialize_init=True,
        effect_name=manifest["effect_name"],
        audio_func_name=manifest["audio_func_name"],
        screen_image=make_cover(manifest["effect_name"], [p["name"] for p in manifest["params"]]),
        gid=manifest["gid"], fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj, output_path=out_zdl,
        fxid_version=manifest["fxid_version"].encode("ascii"),
        flags_byte=manifest.get("flags_byte", 1),
        audio_nop=manifest.get("audio_nop", False),
        dsp_cost=8.0,
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__",
    ))
    print(f"[BBAuto] built {out_zdl}")


if __name__ == "__main__":
    main()
