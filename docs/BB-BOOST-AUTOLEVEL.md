# BB Boost + Auto Level — hardware-test prototype

Status: source prototype added on a separate branch. It is not a release effect
and no target ZDL is claimed until the TI C6000 compiler has built and checked it.

## Signal paths

Physical clean amp:

    Guitar -> BBBoost -> BBAuto (Mode=AMP, transparent) -> amp input

NAM profile:

    Guitar -> BBBoost -> NAM A2 Lite -> BBAuto (Mode=NAM) -> Delay -> Reverb

BBAuto must be after NAM in PROFILE mode because it measures the model's
actual output. Before NAM, it cannot know what level the NAM will produce.
For a real amplifier it remains transparent in AMP mode. Moving the BBAuto
slot to the post-NAM position is therefore part of changing rigs/patches.

## Effects

### BBBoost — pre-amp gain

- Pure gain, before the physical amp or NAM.
- Gain control: unity to approximately +18 dB, quadratic amplitude curve.
- Level control: post-boost trim, normally left at 100 when driving an amp/model.
- No saturation, compressor, envelope, EQ, buffer, or persistent state.
- One shared multiplier for all 16 samples in the stereo block.

### BBAuto — post-NAM auto level

- Mode AMP: exact sample pass-through.
- Mode NAM: stereo-linked peak-envelope downward leveler.
- Amount changes the maximum attenuation.
- 16-byte per-instance state; no delay line, lookahead, FFT, RMS square-root,
  runtime division, math library or heap allocation.
- It is an approximate level stabilizer, not LUFS matching. Dynamic content,
  capture level, NAM Input/Output and playing style affect the result.

The leveler currently uses a fixed threshold and smoothed attack/release values.
Those are intentionally first-pass listening parameters and must be calibrated
on the pedal. The amount control allows the user to reduce the effect if it
pumps or dulls the attack.

## DSP budget

The linker declarations are provisional admission estimates: BBBoost 4 and
BBAuto 8. These numbers are not target cycle measurements. The repository's
NAM A2 Lite full-rate test declares around 194; the project's documented
MS-70CDR DSP-full boundary is around 224.4. NAM + BBBoost + BBAuto therefore
declares about 206 and leaves only about 18.4 for the rest of the chain.

That is a real design constraint: the current effect database's lowest stock
Delay and Reverb estimates are about 15.21 and 16.87 respectively. With those
values, full-rate NAM + both new effects + Delay + Reverb would be about 238.08,
above the observed admission boundary. An experimental NAM Eco variant around
177 would put that particular combination near 221.08, but leaves only about
3.3 units of margin and still requires checking whether those exact stock
effects are available on the target pedal. A custom delay declared at 20 plus
the 16.87 reverb would still exceed the boundary with that Eco figure.

Consequently this branch first makes BBBoost and BBAuto independently
testable. Start with NAM + BBBoost + BBAuto, then add Delay, then Reverb one at
a time. For the full five-effect chain, we may need to merge the small BBAuto
leveler into the custom delay after listening validation, and/or use a lower
cost NAM variant. Never lower descriptor costs just to force an overloaded
chain to be accepted. If the Zoom rejects the chain or audio crackles, remove
the last effect and record the combination.

## Build

Requires the repository's TI C6000 C674x compiler and local stock handler blobs.
From the repository root:

    python3 -B build_all.py bbboost bbauto

The two effects are registered as experimental, not as normal release effects.
The build wrapper moves their ZDL files to build/probes/ and regenerates the
Patch Editor effect database, keeping them out of the normal release pack.

To run the host-side DSP helper tests:

    cc -std=c99 -O2 -Wall -Wextra -Werror src/custom/bbboost/tests/test_dsp.c -lm -o /tmp/bbboost-dsp-test
    /tmp/bbboost-dsp-test

## First hardware test

1. Back up the pedal's patches and current ZDL files before installing.
2. Install BBBoost and BBAuto from build/probes/.
3. First test BBBoost alone before a clean physical amp. Set Level=100 and
   increase Gain gradually from 0. Confirm unity at 0 and stronger amp breakup
   as Gain rises. Reduce the amplifier's own volume if necessary.
4. For NAM, use the intended A2 Lite profile and this order:
   BBBoost -> NAMLite -> BBAuto -> Delay -> Reverb.
5. Set BBAuto Mode=AMP first. It must sound unchanged compared with BBAuto
   bypassed. Then set Mode=NAM and start Amount around 60.
6. Start with BBBoost Gain=0. Raise it gradually. Compare NAM output with
   BBAuto off/AMP and NAM mode. Adjust Amount for the desired level stability.
7. Add Delay and Reverb separately. Check DSP-full refusal, crackles, patch
   save/reload, bypass, and mode switching at sustained notes and hard attacks.

Do not use high listening volume for the first tests. If the pedal freezes,
follow the existing recovery procedure; do not repeatedly power-cycle a
known-frozen experimental ZDL.

## Known boundaries

- BBBoost is an input gain stage, not an overdrive. It relies on the following
  real amp or NAM model to create crunch.
- BBAuto can only compensate a downstream signal when it is placed after that
  processor. No data is passed between ZDL effect instances.
- The NAM A2 Lite is still experimental in this repository. Earlier compact
  builds loaded but had a bitcrushed/noisy character; this branch does not claim
  to fix or replace the NAM engine.
- The TI cross-compiler is not bundled. A host test cannot replace target build,
  ZDL structural validation, or an actual MS-70CDR listening test.
