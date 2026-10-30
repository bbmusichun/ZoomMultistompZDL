# G1Xon BB Drive — implementation plan

## Target

A custom ZDL for the Zoom G1Xon, developed in the user's fork. This is a new effect, not a modification of the upstream release pack.

**Compatibility is not yet proven.** The repository documents the ZDL family as a shared format, but no confirmed custom-ZDL installation on a G1Xon is recorded. The G1Xon DSP budget, per-effect state memory, and recovery procedure are unknown. Do not install a candidate on the pedal until the identity/patch-read probes and recovery path have been checked.

## Agreed sound and controls

The effect combines a clean/tape character stage, a switchable booster, a three-band EQ, and a simple delay.

| Control | Intended range / choices | Notes |
|---|---|---|
| Character | CLEAN / TAPE | CLEAN is transparent; TAPE adds restrained warmth/saturation |
| Booster level | 0 to +12 dB | Separate from the character switch |
| Booster position | PRE / POST | PRE drives the character/EQ/delay chain; POST raises the complete wet/dry output, including repeats |
| Bass | Cut/boost | Three-band tone control |
| Mid | Cut/boost | Three-band tone control |
| Treble | Cut/boost | Three-band tone control |
| Delay Time | Short to long; exact range TBD | Must be set against the G1Xon's measured state-memory budget |
| Echo Mix | 0–100% | Wet/dry balance for the delay |

No feedback control and no envelope follower in the first version.

The control count is eight, within the repository's documented nine-control ceiling. The exact EQ ranges, delay range, parameter curves, and character voicing will be fixed in implementation after the UI and parameter scaling are verified.

## Signal-flow definition

- PRE: booster → character → EQ → delay → output mix.
- POST: character → EQ → delay → booster on the final output.
- CLEAN bypasses only the tape coloration; it does not bypass the EQ, delay, or selected booster position.
- Delay feedback is fixed at zero in v1 (single repeat). This keeps the first delay implementation bounded and avoids an unrequested feedback control.

## Safe development gates

1. **Read-only G1Xon discovery:** confirm MIDI identity and collect a patch dump. Do not send undocumented SysEx writes, patch writes, or firmware commands.
2. **UI-only smoke ZDL:** preserve the final eight-control descriptor shape while using `audio_nop: true`. Confirm the build, ZDL metadata, unique effect ID/name, and binary size offline. Do not treat an emulator pass as proof of firmware readiness.
3. **Minimal pass-through DSP:** add only the verified audio-context shuttle and a dry pass-through. Inspect disassembly, relocations, sections, external symbols, and binary size.
4. **Scalar stages:** add booster and simple EQ one at a time. Keep the implementation free of switch/jump tables, runtime math-library calls, heap, static state, and unverified helpers.
5. **Character stage:** add restrained tape coloration only after clean/boost/EQ behavior is stable.
6. **Delay memory probe:** determine whether the G1Xon exposes a usable state arena and measure its bounds before allocating any delay line. Start with a deliberately short buffer; never assume the MS-70CDR memory budget applies.
7. **Hardware validation:** only after the previous gates pass and the user's G1Xon recovery path is independently confirmed. Test one effect in an expendable patch with a backed-up pedal.

## Repository-specific constraints

Follow `docs/SAFE-DSP-RULES.md` and `docs/pedal-research/zdl-family.md`:

- Preserve the `ctx[11]/ctx[12]` magic shuttle exactly as the known-good effects do.
- No `switch` or dense integer dispatch in audio code.
- No uninitialized static arrays, `.bss`, `.usect`, heap allocation, or large `.fardata`.
- Avoid runtime math-library calls and unverified `__c6xabi_*` helpers.
- Keep the ZDL basename at eight characters or fewer and verify no collision.
- Do not reuse the MS-70CDR DSP-cost or state-memory limits as G1Xon facts.
- Do not add the candidate to the normal release list until it has passed the G1Xon hardware gates.

## Current status

- [x] User selected CLEAN/TAPE character and PRE/POST booster placement.
- [x] User approved booster + EQ + simple delay as the first feature set.
- [x] Feedback and envelope follower excluded from v1.
- [x] Fork access verified; push permission is available.
- [ ] G1Xon MIDI identity and patch layout verified.
- [ ] Eight-control UI-only smoke build.
- [ ] Pass-through and scalar DSP checks.
- [ ] G1Xon state-memory probe and delay implementation.
- [ ] Hardware installation and listening test.

No ZDL has been built or tested on hardware in this initial planning commit.
