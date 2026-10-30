# G1Xon BB Delay — implementation plan and status

## Target

A custom ZDL effect named **BB Delay**, developed in the user's fork on branch
g1xon-bbdrive-prototype. It is separate from the upstream release pack.

**Compatibility is not yet proven.** The repository documents the ZDL family as
a shared format, but no confirmed custom-ZDL installation on a G1Xon is
recorded. The G1Xon DSP budget, per-effect state memory and recovery procedure
remain unknown.

## Agreed sound and controls

The first version combines a clean/tape character stage, a switchable booster,
a three-band EQ and a simple single-repeat delay.

| Control | Range / choices | Function |
|---|---|---|
| Char | CLEAN / TAPE | CLEAN is transparent; TAPE adds restrained saturation |
| Boost | OFF / ON | Independent booster switch |
| Pos | PRE / POST | PRE drives the character/EQ/delay; POST raises the full output, including echo |
| Level | 0 to +12 dB | Booster amount |
| Bass | Approx. -6 to +6 dB | Low-band EQ |
| Mid | Approx. -6 to +6 dB | Mid-band EQ |
| Trebl | Approx. -6 to +6 dB | High-band EQ |
| Time | 20 ms to arena-dependent maximum | Delay time |
| Echo | 0–100% | Dry/wet mix |

Feedback and envelope follower are excluded from v1. The delay records the
current processed input only, so there is one repeat and no recirculating
feedback loop.

## Signal flow

- PRE: booster → character → EQ → delay → dry/wet mix.
- POST: character → EQ → delay → dry/wet mix → booster.
- CLEAN bypasses only the tape coloration.
- PRE boost is recorded into the delay; POST boost raises both dry and echo
  output after the mix.

## Implementation now in the fork

- **src/custom/bbdelay/bbdelay.c** — stereo single-repeat delay DSP, PRE/POST
  booster, CLEAN/TAPE saturation and three-band EQ.
- **src/custom/bbdelay/manifest_pedal.json** — nine-control descriptor; category
  Delay (gid=8), effect ID 915, output file basename BBDelay.
- **src/custom/bbdelay/build.py** — build script with a default UI-only smoke
  mode and opt-in DSP mode.
- **src/custom/bbdelay/README.md** — controls, build commands and safety limits.

The gid=8, fxid=915 pair was checked against the checked-in
tools/effects_db.json before creation. The plugin is not added to
build_all.py or the release list.

## DSP design and bounds

The effect uses only the memory reported through ctx[3]. It checks the
descriptor's base, end and span before accessing the state area, then selects
a power-of-two stereo ring of 8,192, 16,384 or 32,768 samples per channel.
The ring requires approximately 64, 128 or 256 KiB, plus the state header.
If the smallest ring does not fit, the DSP leaves the signal unchanged.
The ring is cleared incrementally in 256-sample chunks.

The delay time maps from 20 ms to the maximum supported by the detected ring.
The exact delay range therefore depends on the memory the target firmware
actually provides. No G1Xon memory size is assumed.

The audio code avoids switch statements, dynamic allocation, static delay
arrays, modulo, runtime math-library calls and float-to-unsigned conversion.
The Zoom ctx[11]/ctx[12] magic shuttle is preserved. The custom _init
parameter-materialization path is disabled for the unverified G1Xon target.

## Safe build procedure

Default build:

~~~sh
python3 src/custom/bbdelay/build.py
~~~

This creates a UI-only ZDL with audio_nop=True; it is not an audible delay.
The actual DSP build is opt-in:

~~~sh
BBDELAY_ENABLE_DSP=1 python3 src/custom/bbdelay/build.py
~~~

Do not install the DSP-enabled binary until ZDL loading, the G1Xon state arena
and a recovery procedure have been independently verified. The remaining
seven synthesized knob handlers have not yet been hardware-proven on this
target. A host build or emulator pass cannot establish G1Xon readiness.

## Status

- [x] CLEAN/TAPE character and PRE/POST booster placement defined.
- [x] Booster, Bass/Mid/Treble EQ and simple Delay feature set defined.
- [x] Feedback and envelope follower excluded from v1.
- [x] Fork write access verified.
- [x] Effect source and state-arena guard added.
- [x] Nine-control pedal manifest added.
- [x] Build script added; default is UI-only and DSP is opt-in.
- [ ] TI C6000 compile and offline ZDL/linker inspection.
- [ ] G1Xon MIDI identity and patch layout verified (read-only).
- [ ] G1Xon state-arena size and descriptor semantics verified.
- [ ] Synthesized handlers for later controls hardware-verified.
- [ ] DSP-enabled ZDL installed and audio-tested with a confirmed recovery path.

No ZDL binary has yet been built or tested on a G1Xon. No hardware installation
or firmware write has been performed.
