# ZD2 port research — September 14, 2026

Status: notes only. No ZD2 build, effect port, dependency installation, or pedal
write was performed for this investigation. Our released pack remains ZDL-only;
PE has not gained Plus-series patch support.

## New evidence

[zoom-zt2 issue 93](https://github.com/mungewell/zoom-zt2/issues/93) starts with
extracting and disassembling LineSel in 2025. The later July 2026 discussion
links to [Stomphacks](https://github.com/thammer/stomphacks), a working C-to-ZD2
pipeline. Its author reports hardware testing of gain and three routing effects
on MS-70CDR+ units. This is upstream evidence, not a test performed by us.

Stomphacks generates the effect container and code, uses zoom-zt2 for file/MIDI
handling, and uses TI C6000 tools 8.5.0, the compiler version already installed
for this repository. Its existence supersedes our earlier question about
whether custom ZD2 effects are possible on that model. It does not make ZDL
binaries interchangeable with ZD2 or prove support for every Plus pedal.

## Update — October 19, 2026

Stomphacks as of commit `2647049` (2026-10-15); still notes only, nothing
built or installed by us.

- **Effect IDs are now namespaced per author**
  ([effect-ids.md](https://github.com/thammer/stomphacks/blob/main/docs/effect-ids.md),
  [id-scheme.md](https://github.com/thammer/stomphacks/blob/main/docs/id-scheme.md),
  [effect-authors.json](https://github.com/thammer/stomphacks/blob/main/ids/effect-authors.json)).
  The 32-bit ID at container offset 0x60 is `group << 24 | low16`. The top
  byte is the browser category and differs per model (MS-70CDR+: 06
  MODULATION, 07 SFX, 08 DELAY, 09 REVERB, ...), so a multi-model port is one
  file per model. Stock effects stay below low16 0x2000. Own-pedal builds use
  the playground, low16 0x1000-0x13ff, with an `XX` filename prefix;
  0x1400-0x1fff is reserved. Anything distributed needs an assigned range at
  0x2000 or above and a 2-letter filename prefix, requested by an issue on
  that repo. Assigned so far: Thomas Hammer 0x2000-0x23ff (WF), Alexander Lim
  (Leemuzhko) 0x3000-0x33ff (LM). **Before we distribute any ZD2 port, request
  a range and prefix** (asking the owner first: it is a post on GitHub).
- **Filenames are a second namespace.** The pedal stores effects by an
  8-character flash name, and installing over an existing name silently keeps
  the old file. Same lesson as our 8-character ZDL basenames.
- **Safer installer:** it reads the pedal's effect list first and refuses an
  ID or filename that is already installed, and refuses an install that would
  leave less than 100 KB free.
- **Tuner knob reset:** when the tuner closes, the MS Plus firmware writes a
  zero into one coefficient slot; their build tool had mapped the first knob
  there. Knobs now start at slot 5. Worth checking whether the ZDL pedals do
  anything similar to a first knob (TODO.md).
- **Momentary footswitch:** an effect can expose a trailing LATCH / UNLATCH
  toggle; the firmware does the rest, and the kernel only sees the bypass
  ramp (hardware-validated on an MS-70CDR+, 2026-07-25). Stock ZDL effects
  with the same toggle (e.g. EXCITER) suggest a ZDL equivalent may exist;
  unexplored here. Relevant to Loop and Stasis.
- **Routing family** (WfSplit / WfSwap / WfMerge, 2026-07-22): two mono
  chains on one pedal, with separate outputs per group, an A/B footswitch or
  parallel blends. Never with a stock LINE SEL in the same patch. The MS Plus
  counterpart of our NAM / CabIR split outputs.
- One-click installs of shared effects: https://sym.bios.is.

## Interface findings relevant to our pack

The [upstream ABI documentation](https://github.com/thammer/stomphacks/blob/main/docs/zd2-abi.md)
reports the following for MS-70CDR+ firmware 1.20:

- Audio receives separate instance and context pointers; the effect bus is
  `ctx[1]`. Blocks contain 16 frames per channel at 44.1 kHz, compared with our
  ZDL kernels' 8-frame channel blocks. Port the entry adapter and loop sizes.
- Coefficients are in `instance[1]`; small per-instance state is in
  `instance[2]`. The documented proven state floor is 192 bytes, not a maximum.
- Init invokes every edit handler to restore stored parameter values, including
  after chain edits. This closely parallels our parameter-materialization fix.
- The documented relocations are ABS32, ABS_L16 and ABS_H16. Runtime helper
  references must resolve inside the effect. Our linker is not a drop-in ZD2
  builder merely because it handles related C6000 relocations.
- Delay-line memory at `instance[3]` is outside that document's established
  contract. The issue's suggestion that it resembles our three-word ZDL buffer
  descriptor is explicitly unconfirmed for Plus hardware. Do not assume our
  observed arena size transfers to ZD2.

Our older `build/ABI.md` LineSel cross-reference predates this documentation.
Its register-derived context labels should be reconciled with the separate
instance/context model before implementation, not copied into a port verbatim.

The [routing writeup](https://github.com/thammer/stomphacks/blob/main/docs/chain-routing.md)
and issue discussion also describe Split/Swap/Merge using the output bus as a
parallel path. That is an optional future feature, separate from porting our DSP.

## Proposed sequence — not started

1. Inspect and pin an upstream Stomphacks revision (`2647049` or later);
   reproduce its gain build locally and review the generated validation
   report. Test ports in the playground ID range with an `XX` prefix.
2. Compare its entry points, coefficients, bypass and init behavior with ours.
3. Select a small effect after measuring its state requirements; port and test
   that one before considering a whole-pack conversion.
4. Establish delay-memory ownership, size and isolation across multiple slots
   before Stasis, Rooms, Spool or other buffer-heavy effects.
5. Investigate PE's Plus-series patch protocol independently, using documented
   commands or captured traffic rather than guessing command bytes.
6. Before distributing anything, request an author ID range and filename
   prefix from Stomphacks, and build one file per target model.

Before any hardware trial, review the upstream
[safety procedure](https://github.com/thammer/stomphacks/blob/main/SAFETY.md).
The author reported bricked units during development. Its procedure centers on
confirmed autosave-off, backups, scratch patches, readback comparison and cleanup.
No hardware trial is authorized or implied by recording these notes.
