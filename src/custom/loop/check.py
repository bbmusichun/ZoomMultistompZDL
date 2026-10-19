#!/usr/bin/env python3
"""Host check for loop.c: a scripted session (record, play, overdub, undo,
redo, stop, clear), every output sample compared with the loop it should hold.
    python3 src/custom/loop/check.py [--long]
--long points the Long regions at host buffers to check the segment layout."""
import argparse, ctypes as C, subprocess, sys, tempfile
from pathlib import Path
import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SR, BLK = 44100, 8


def load(long_mode):
    td = Path(tempfile.mkdtemp())
    stub = td / "stub.c"
    stub.write_text("unsigned int _Fx_DLY_Loop_Info[64] = {0, 1, 0, 128, 64, 0x12345678u};\n"
                    "unsigned char loop_r1[614400 + 64], loop_r2[425984 + 64], loop_r3[1677952 + 64];\n")
    defs = []
    if long_mode:
        defs = ["-DLOOP_TEST_LONG", "-DLONG_R1=((uintptr_t)loop_r1)", "-DLONG_R2=((uintptr_t)loop_r2)",
                "-DLONG_R3=((uintptr_t)loop_r3)",
                "-include", "stdint.h", "-Dloop_decl=1"]
        stub2 = td / "decl.h"
        stub2.write_text("extern unsigned char loop_r1[], loop_r2[], loop_r3[];\n")
        defs += ["-include", str(stub2)]
    so = td / "loop.so"
    subprocess.run(["cc", "-O2", "-shared", "-fPIC", "-w", "-DZV_HOST", *defs, f"-I{ROOT}/src/custom/common",
                    f"-I{HERE}", f"-I{ROOT}/src/airwindows/common", str(HERE / "loop.c"), str(stub), "-o", str(so)], check=True)
    return C.CDLL(str(so))


def run(lib, x, events, knobs, arena_kb=705):
    fn = lib.Fx_DLY_Loop; fn.argtypes = [C.POINTER(C.c_size_t)]
    arena = (C.c_uint8 * (arena_kb << 10))(); base = C.addressof(arena)
    desc = (C.c_size_t * 2)(base, base + (arena_kb << 10))
    params = (C.c_float * 16)(1, 0, 0, 0, 0.01, *[k / 100 for k in knobs])
    fx = (C.c_float * 16)(); src = C.c_uint(7); dst = C.c_uint(); dp = C.c_size_t(C.addressof(dst))
    ctx = (C.c_size_t * 16)()
    for k, v in [(1, params), (3, desc), (5, fx), (11, dp), (12, src)]:
        ctx[k] = C.addressof(v)
    nb = len(x) // BLK
    out = np.zeros(nb * BLK, np.float32)
    ev = {}
    for b, what in events:
        ev.setdefault(b, []).append(what)
    for b in range(nb):
        for what in ev.get(b, []):
            if what == "press":
                params[0] = 0.0 if params[0] > 0.5 else 1.0
            else:                                           # ("knob", index, value)
                params[5 + what[1]] = what[2] / 100
        for j in range(BLK):
            fx[j] = fx[j + 8] = float(x[b * BLK + j])
        fn(ctx)
        assert fx[0:8] == fx[8:16], "L and R should match for a mono input"
        out[b * BLK:(b + 1) * BLK] = fx[0:8]
    return out


def blk(t):
    return int(t * SR) // BLK


def long_take(lib, long_mode, seconds):
    """Record `seconds` of noise (Long: across the memory regions), then play
    it back twice; a recording past the capacity closes itself at the cap."""
    rng = np.random.default_rng(3)
    nb = blk(seconds + 0.5) + 2 * blk(seconds) + 100
    x = np.zeros(nb * BLK, np.float32)
    P1, P2 = blk(0.5), blk(0.5 + seconds)
    x[P1 * BLK:P2 * BLK] = (0.25 * rng.standard_normal((P2 - P1) * BLK)).astype(np.float32)
    y = run(lib, x, [(P1, "press"), (P2, "press")], [100, 0, 100, 0, 1 if long_mode else 0])   # Dry 0: only the loop
    xb, yb = x.reshape(-1, BLK), y.reshape(-1, BLK)
    # where did the loop close? the first block after P1 that plays back block P1's audio
    rec = xb[P1:P2]
    k0 = next(k for k in range(P1 + 1, nb) if np.abs(yb[k] - rec[0]).max() < 1e-3 and np.abs(yb[k]).max() > 0)
    L = k0 - P1
    exp = np.array([rec[(k - k0) % L] for k in range(k0, nb)])
    err = np.abs(yb[k0:] - exp).max()
    print(f"{'Long' if long_mode else 'normal'} take of {seconds} s: loop {L * BLK / SR:.2f} s, worst error {err:.2e}")
    return err < 1e-3


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--long", action="store_true"); a = ap.parse_args()
    lib = load(a.long)
    if not long_take(lib, a.long, 12.0 if a.long else 8.0):
        print("FAIL"); sys.exit(1)
    n = blk(22.0) * BLK
    t = np.arange(n) / SR
    A = (0.3 * np.sin(2 * np.pi * 220 * t) * ((t >= 0.5) & (t < 2.5))).astype(np.float32)
    B = (0.2 * np.sin(2 * np.pi * 330 * t + 1) * ((t >= 5.0) & (t < 6.0))).astype(np.float32)
    x = A + B
    P1, P2, P3, P4 = blk(0.5), blk(2.5), blk(5.0), blk(6.0)
    U1, U2 = blk(9.0), blk(11.0)
    S1, S2 = blk(13.0), blk(13.2)                         # double press: stop
    G = blk(14.0)                                         # press: play from the top
    C1, C2 = blk(16.0), blk(16.2)                         # stop again; then clear (double while stopped)
    C3, C4 = blk(17.0), blk(17.2)
    R1 = blk(18.0)                                        # press after clear: a new recording starts
    events = [(P1, "press"), (P2, "press"), (P3, "press"), (P4, "press"), (U1, ("knob", 3, 1)), (U2, ("knob", 3, 0)),
              (S1, "press"), (S2, "press"), (G, "press"), (C1, "press"), (C2, "press"), (C3, "press"), (C4, "press"),
              (R1, "press")]
    y = run(lib, x, events, [100, 100, 100, 0, 1 if a.long else 0])
    xb, yb = x.reshape(-1, BLK), y.reshape(-1, BLK)
    L = P2 - P1
    loopA = xb[P1:P2].copy()
    loopAB = loopA.copy()
    for k in range(P3, P4):                               # the overdub lands at loop position (k - P2) % L
        loopAB[(k - P2) % L] += xb[k]
    exp = xb.copy()
    pass_end = P3 + L                                     # one full lap after the overdub began
    for k in range(P2, nb := len(xb)):
        if S2 <= k < G or C2 <= k < C3 or k >= C4:        # stopped / cleared / recording again
            continue
        j = (k - P2) % L if k < G else (k - G) % L if k < C3 else (k - C3) % L   # the first press of a double plays
        if k < pass_end:
            src = loopA
        elif U1 <= k < U2:
            src = loopA                                   # undone
        else:
            src = loopAB
        exp[k] += src[j]
    err = np.abs(yb - exp)
    worst = err.max()
    print(f"loop {L * BLK / SR:.2f} s; worst error {worst:.2e} (12-bit block float: ~2e-4 expected)")
    for name, lo, hi in [("record", P1, P2), ("play", P2, P3), ("overdub", P3, pass_end), ("play A+B", pass_end, U1),
                         ("undo", U1, U2), ("redo", U2, S1), ("stopped", S2, G), ("play again", G, C1), ("stopped 2", C2, C3),
                         ("cleared", C4, nb)]:
        print(f"  {name:10s} worst {err[lo:hi].max():.2e}")
    if worst > 1e-3:
        bad = int(np.argmax(err.max(1)))
        print("FAIL at block", bad, "t=%.3f s" % (bad * BLK / SR)); sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
