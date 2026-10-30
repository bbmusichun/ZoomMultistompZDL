# BB Delay

Custom single-repeat delay for the G1Xon development branch.

## Controls

1. **Char** — CLEAN / TAPE. TAPE adds gentle saturation; CLEAN bypasses only this stage.
2. **Boost** — independent booster ON/OFF.
3. **Pos** — PRE / POST. PRE drives the character, EQ and delay input. POST raises the complete dry/wet output, including the echo.
4. **Level** — booster amount, 0 to +12 dB.
5. **Bass / Mid / Trebl** — three-band EQ with a centre-neutral position and an approximate ±6 dB range.
6. **Time** — 20 ms to the maximum supported by the detected state arena.
7. **Echo** — dry/wet mix.

There is no feedback control in v1. The delay buffer is written from the current processed input only, so it produces one repeat.

## Build modes

The default build is deliberately UI-only:

~~~sh
python3 src/custom/bbdelay/build.py
~~~

It produces dist/BBDelay.ZDL with audio_nop enabled. This is a loader/UI smoke candidate, not an audible delay.

The DSP build is opt-in:

~~~sh
BBDELAY_ENABLE_DSP=1 python3 src/custom/bbdelay/build.py
~~~

Do not install the DSP-enabled candidate until the target G1Xon's ZDL loading, state arena and recovery path have been independently checked.

BB Delay is not added to build_all.py or the release list.

## Memory and compatibility

The DSP reads the ctx[3] state descriptor and checks its base, end and span before using it. It chooses a stereo ring of 8,192, 16,384 or 32,768 samples per channel, depending on the available arena. Ring memory is approximately 64, 128 or 256 KiB, plus the state header. If the minimum ring does not fit, the DSP leaves the signal dry.

The exact ctx[3] descriptor contract and actual memory available on the G1Xon are not confirmed. A successful host build or emulator run is not proof that the pedal will load or run it. No G1Xon installation, hardware validation or listening test is claimed.

## Implementation gates

- The UI-only smoke build leaves the later seven knob handlers as NOP stubs, so it cannot be used to change those controls. The DSP-enabled candidate switches on the repository's synthesized compact handlers for those seven controls; that handler path is not yet hardware-proven on this target. The first two use the stock LineSel pair.
- Custom init parameter materialization is disabled for the G1Xon candidate.
- DSP cost 27.0 is a provisional estimate, not a G1Xon measurement.
- The effect remains on g1xon-bbdrive-prototype and must not enter the normal release pack before hardware validation.
