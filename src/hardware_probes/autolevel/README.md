# AutoLvl — post-amp automatic level rider

Experimental ZDL probe. Place it **after** the amp model / NAM / drive stage.
It cannot measure or compensate an effect placed after it.

## Signal path

`Amp/NAM/Drive -> AutoLvl -> time/modulation effects -> output`

AutoLvl uses one linked-stereo envelope follower and one slow feedback gain
controller. The same gain is applied to left and right, so stereo balance is
preserved. It aims to keep the output RMS level near the Target setting
when the preceding amp/drive stage changes loudness.

## Controls

- **Target** — target mean-absolute signal level. Start at 45.
- **Resp** — how quickly the gain rider corrects level changes. Start at 35;
  lower is smoother, higher reacts faster.

## DSP choices

- No division, libm, FFT, oversampling, delay line, EQ, clipping or compressor
  sidechain.
- RMS is estimated from mean-square power; no square root or division is used.
- Per-instance state is 16 bytes in the descriptor arena.
- Gain is limited to 0.05x..4x. During near-silence it relaxes toward unity
  rather than amplifying the noise floor.
- Declared DSP cost is 8.0 as an initial conservative estimate, not a measured
  hardware benchmark.

This is a level rider, not a brick-wall limiter. It will not guarantee identical
perceived loudness for every possible guitar phrase, and it can change dynamics
if Resp is set too high. A fixed Target and moderate Resp are the intended
starting point.

## Build

From repository root, with TI C6000 CGT 8.5.0 installed at the path used by
this repository:

```sh
python3 src/hardware_probes/autolevel/build.py
```

Output: `build/probes/autolevel/AutoLvl.ZDL`. It is intentionally not copied
to `dist/` and is not added to the release pack.

## First hardware test

1. Build and install `AutoLvl.ZDL` with Effect Manager.
2. Put it immediately after the amp model/NAM in the patch.
3. Use the same guitar phrase and compare the amp at clean and driven settings.
4. Start with Target 45 / Resp 35. Adjust Target for the overall output level.
5. Increase Resp only if the level correction is too slow. Reduce it if the
   sound pumps or swells between notes.
6. Compare bypass and engaged at matched listening loudness; listen for pumping,
   transient loss, noise lift, stereo changes, crackles or freezes.

A successful build is not proof of DSP headroom or pedal stability. This probe
must be tested alone with a simple amp patch before being combined with NAM and
other effects.
