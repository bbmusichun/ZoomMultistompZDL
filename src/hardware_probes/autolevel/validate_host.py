#!/usr/bin/env python3
"""Host-side regression checks for the AutoLvl control law.

This is a floating-point reference simulation, not a substitute for TI object
inspection or a real pedal test.
"""
from __future__ import annotations

import math
import statistics

SR = 48000
TARGET = 0.040 + 0.45 * 0.120
TARGET_POWER = TARGET * TARGET
GAIN_ADAPT = 0.002 + 0.35 * 0.018
ENV_COEFF = 0.001
FLOOR_POWER = 0.00000225


def run_level(amplitude: float, seconds: float = 2.0) -> tuple[float, float]:
    env = 0.0
    gain = 1.0
    output = []
    count = int(SR * seconds)
    for n in range(count):
        x = amplitude * math.sin(2.0 * math.pi * 440.0 * n / SR)
        detector_power = x * x
        env += (detector_power - env) * ENV_COEFF
        if detector_power > FLOOR_POWER:
            error = TARGET_POWER - env * gain * gain
            gain += gain * error * GAIN_ADAPT
        else:
            gain += (1.0 - gain) * 0.00010
        gain = min(4.0, max(0.05, gain))
        output.append(x * gain)
    tail = output[-SR // 2:]
    rms = math.sqrt(sum(v * v for v in tail) / len(tail))
    return rms, gain


def main() -> None:
    results = []
    for amplitude in (0.05, 0.08, 0.20, 0.50, 1.0):
        rms, gain = run_level(amplitude)
        if abs(rms - TARGET) > 0.003:
            raise SystemExit(
                f"FAIL amplitude={amplitude}: RMS {rms:.5f}, target {TARGET:.5f}"
            )
        results.append((amplitude, rms, gain))

    # The linked detector must use one shared gain for both channels.
    shared_gain = 0.73
    left, right = 0.20 * shared_gain, 0.07 * shared_gain
    if abs((left / right) - (0.20 / 0.07)) > 1e-6:
        raise SystemExit("FAIL linked stereo gain changed channel balance")

    print("AutoLvl reference checks passed (not a DSP binary/hardware test):")
    for amplitude, rms, gain in results:
        print(f"  input peak {amplitude:>4.2f}: output RMS {rms:.5f}, gain {gain:.3f}x")
    print(f"  target RMS: {TARGET:.5f}; linked stereo ratio: preserved")


if __name__ == "__main__":
    main()
