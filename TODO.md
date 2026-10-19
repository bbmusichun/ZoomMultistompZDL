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
- **Pedal tests pending:** Skull 0.05 and full-screen Scope 0.03 / Spectrum
  0.02 / Ring 0.02 (no knob boxes, also after switching patches); NAM split
  rebuilt with `restrict` back (amp alive on both sides).

## Later

- **MIDISTOMP and a DSP limit of 300** (reported for an upcoming version):
  once released, measure the real limit on the MS-70CDR with the cost probes
  and update PE's meter (`DSP_BUDGET`, 228 now) and the README budget table.
- **FXID collision:** Matujuice Sweep (496) has the same FXID as the stock
  ANA234Cho (reported by `build/extract_effect_db.py`). His ID; tell him, and
  check what the pedal does with both installed.
- **Issue #6:** Patch Editor reads banks only up to E on a G1Xon (pedal has
  A-J).
- **NAM Input default from capture metadata** (needs a level measurement, a
  meter effect).
- **Eco+ level tracking** (the correction EQ is fitted at three playing
  levels; follow the level instead).
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
