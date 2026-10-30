#!/usr/bin/env python3
"""Build the isolated post-amp AutoLvl hardware probe.

Run from repository root:
    python3 src/hardware_probes/autolevel/build.py

Output is build/probes/autolevel/AutoLvl.ZDL, not dist/.
"""
from __future__ import annotations

import json
import subprocess
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))

from linker import LinkerConfig, ObjFile, link, params_from_manifest
from custom_covers import make_cover

TI_ROOT = Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS")
CL6X = TI_ROOT / "bin" / "cl6x"


def main() -> None:
    manifest = json.loads((HERE / "manifest.json").read_text())
    out_dir = ROOT / "build" / "probes" / "autolevel"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = out_dir / "autolevel.obj"
    zdl = out_dir / "AutoLvl.ZDL"

    subprocess.run([
        str(CL6X), "--c99", "-O2", "-mv6740", "--abi=eabi",
        "--mem_model:data=far",
        f"--include_path={TI_ROOT}/include",
        f"--include_path={ROOT / 'src' / 'airwindows' / 'common'}",
        "-c", str(HERE / "autolevel.c"),
        f"--output_file={obj}",
    ], cwd=out_dir, check=True)

    parsed = ObjFile(obj)
    entry = next((s for s in parsed.symbols
                  if s["name"] == manifest["audio_func_name"]), None)
    if entry is None or entry["value"] != 0:
        raise RuntimeError("Audio entry is missing or is not at .audio offset 0")
    if any(s["name"] and s["shndx"] == 0 for s in parsed.symbols):
        raise RuntimeError("Unexpected unresolved helper/runtime symbol")
    for sec in parsed.sections:
        if (sec["name"] in (".text", ".bss", ".far", ".fardata")
                or sec["name"].startswith(".switch")):
            if sec["size"]:
                raise RuntimeError(f"Unsafe outlined/static section: {sec['name']} ({sec['size']})")

    cfg = LinkerConfig(
        effect_name=manifest["effect_name"],
        audio_func_name=manifest["audio_func_name"],
        gid=manifest["gid"],
        fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj,
        output_path=zdl,
        fxid_version=manifest["fxid_version"].encode("ascii"),
        flags_byte=manifest.get("flags_byte", 1),
        screen_image=make_cover("AutoLvl", ["Target", "Resp"]),
        materialize_init=True,
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=2,
        dsp_cost=8.0,
    )
    link(cfg)
    print(f"[AutoLvl] built -> {zdl}")


if __name__ == "__main__":
    main()
