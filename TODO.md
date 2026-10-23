# To do

Open work, roughly in order. Done items move to [CHANGELOG.md](CHANGELOG.md).

## Next

- **CabIR split outputs.** One output through the cab (front of house), the
  other the same signal without it (power amp and a real cab). Same design as
  the NAM split: the side is written by the cab loader into the file (a factor
  table found by a marker), knob 1 (Mix) becomes the no-cab side's level,
  Level / Reso / Pres act on the cab side only. DSP stays 10, file ~16 KB.
  Separate template set (`tools/cab_template_split/`), an Outputs choice in
  the cab loader, the PE knob label, a loader test, emulator vs host check,
  then a test file per side for the pedal. Use with NAM full rate, not NAM
  split (no output would carry the amp).
- **Pedal test pending:** NAM split rebuilt with `restrict` back (amp alive
  on both sides).
- **MIDISTOMP collaboration** (Luca Matturro, 2026-10-20). He offered:
  - an early MIDISTOMP v2.0 build (two buses with routing, parallel
    processing, the 70CDR as two mono pedals; a new effect-chain screen; a
    performance mode), including a build with the DSP limit at 300 (he ran
    chains up to ~293): measure it with the cost probes, then update PE's
    meter and the README;
  - making our effects follow MIDI clock (tempo from MIDISTOMP, as his
    v1.6+ effects do): Loop's Tempo, Hydra, Spiral first; a clock-locked
    Loop could also start and stop with it;
  - Spectrum and Scope as permanent MIDISTOMP menu items (confirmed
    2026-10-23: he will look at the repo after his v2 performance mode);
  - submitting the projects to the Modwerk app.
  - After the v2 release: metering and mastering tools with him (Spectrum,
    a LUFS meter and similar), to make the MS-70CDR an end-of-chain pedal.
  What MIDISTOMP exposes (2026-10-22): a transport block at `0x1181FF00`
  with a magic number, clock count, running flag and tempo
  ([docs/MIDISTOMP-TRANSPORT.md](docs/MIDISTOMP-TRANSPORT.md)). Next: Loop
  first, then Hydra and Spiral.
  - Mac test build M3b7 (mixer, knob 8 fix):
    [midistomp#48](https://github.com/matujuice/zoom-ms-midistomp/issues/48).
    It needs our own copy of Zoom's MS-70CDR 2.10 Mac updater. Back up all
    50 patches first. It includes the raised DSP limit (Luca, 2026-10-23),
    so the DSP cost probes can measure it there.

## Later

- **Clean-room edit handlers.** Every built effect carries knob handlers
  cloned from the stock LineSel effect and a divide routine from a stock
  effect (docs/STOCK-FILES.md). Write our own from the documented ABI
  (docs/EDIT-HANDLER-ABI.md), test them on the pedal, then rebuild `dist/`
  and the NAM/cab templates. Needed before Modwerk can list the effects
  without stock code. Also: the Patch Editor's effect list embeds the stock
  effects' screen pictures.
- **Regenerate the handler blobs** from the user's own stock effects (a
  script), until the clean-room handlers replace them.

- **Install from the browser.** Works per file over USB in normal mode on
  the MS-70CDR 2.10 (2026-10-23, docs/EFFECT-INSTALL-PROTOCOL.md): back up
  all files, replace an effect, add one (file plus its effect-list record),
  remove one; each checked on the pedal after a power cycle. Next: an
  install page in the Patch Editor (and for Modwerk) with the same guards as
  `tools/file_probe.html`: our effects only, a backup first, effect-ID and
  name checks against what is on the pedal, read back after every write,
  the effect file before the list. Then MS-50G, MS-60B and G1on tests. The
  full-reflash path (update mode, `tools/zoom_fs.py`) stays for recovery.
  Later: a Modwerk base firmware (Janik's idea, not a priority).

- **Emulator: NAM split** still differs from the host (amp side near
  silent) while playing correctly on the pedal: one more Ziddle gap, not yet
  found, after the three fixed ones (docs/EMULATOR-TESTING.md).
- **MIDISTOMP and a DSP limit of 300** (reported for an upcoming version):
  once released, measure the real limit on the MS-70CDR with the cost probes
  and update PE's meter (`DSP_BUDGET`, 228 now) and the README budget table.
- **FXID collision:** Matujuice Sweep (496) has the same FXID as the stock
  ANA234Cho (reported by `build/extract_effect_db.py`). Told Luca
  (2026-10-23); he will fix the IDs when he redoes his effects in stereo.
  Until then, check what the pedal does with both installed.
- **Issue #6:** Patch Editor reads banks only up to E on a G1Xon (pedal has
  A-J).
- **NAM Input default from capture metadata** (needs a level measurement, a
  meter effect).
- **Eco+ level tracking** (the correction EQ is fitted at three playing
  levels; follow the level instead).
- **Maybe: a cloud reverb** (a big, diffuse ambient reverb, BigSky-style).
  Paused 2026-10-22; check the DSP budget first.
- **Live-screen effects take their effect ID from the build.** Loop, Scope,
  Spectrum, Ring and Skull find their slot by an `FXID` written in their C
  source, so a renumbered copy shows only its cover. Pass it from the
  manifest (`-DFXID=`) instead.
- **Tuner knob reset check.** Stomphacks found that MS Plus pedals write a
  zero into one coefficient slot when the tuner closes (it reset their first
  knob). Check whether the ZDL pedals do anything similar to our first knob.

## MS Plus (ZD2), if we port

- [Stomphacks](https://github.com/thammer/stomphacks) (Thomas Hammer, MIT) has
  a working C-to-ZD2 pipeline and safe installer for the MS-70CDR+. Since our
  notes (docs/ZD2-PORT-NOTES.md, 2026-10-06) it added a per-author effect-ID
  scheme: distributed effects need an assigned ID range (low16 >= 0x2000) and
  a 2-letter filename prefix, requested by an issue on that repo (Alexander Lim
  has 0x3000-0x33ff / LM). Request one before distributing any ported effect.
  Playground IDs (0x1000-0x13ff, `XX` prefix) are fine for own-pedal tests.
- Their WfSplit / WfSwap / WfMerge routing runs two chains on one MS-70CDR+;
  relevant to split-output ideas there.
- Momentary (latch/unlatch) footswitch support for ZD2 effects: a model for
  Loop / Stasis on MS Plus.
