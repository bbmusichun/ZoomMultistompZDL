# Changelog

What changed, newest first. The project has no version numbers; entries are
dated. Effects in [dist/](dist/) carry their own version in the file
(shown in Zoom Effect Manager).

## 2026-10-18

### NAM + Cab Loader
- **216 capture slots and 216 cab slots** (was 16 each). Slots 1-16 keep
  their effect IDs; captures 17-216 are FXIDs 500-699, cabs 700-899. Slots
  past 16 are made in the browser from one base template per engine
  (`tools/slot_identity.js`); tests check them byte for byte against
  linker-built files.
- **Your name and an icon on the pedal screen.** Pick one of 21 icons (amp,
  cabinet, speaker, tube, mic, skull, flame, ghost…); the pedal-screen picture
  shows it next to the name you typed, and the loader gives you a matching
  Effect Manager thumbnail (`tools/cover_art.js`, `build/capture_icons.py`).
  Every template reserves 1,024 bytes for this picture. Re-export a capture
  to get its name on screen; older files keep working.
- Amp and cab cards now work the same way: name, icon, slot picker and
  preview, with the preview hidden until a file is chosen.
- New filename rules: `NAM<slot><name>` (4/3/2 name letters for slots
  1-9/10-99/100-216); cabs `C<slot><name>` (5 letters, 4 from slot 100).
  Cab names must start with a letter; digit-first names are migrated.

### Patch Editor
- Knows all 432 generated slots, and draws each capture's and cab's own
  name and icon on its card.

### Toolchain
- **Linker fix: calls between code sections.** A call from one code section
  to a function in another landed past its target (the instruction's offset
  was added twice). This froze the pedal when the Blink probe was selected,
  and probably explains an older report on another effect. The linker now
  handles it, or refuses to link when it can't be sure, following Leemuzhko's
  SDK notes. Tonic rebuilds byte-identical.
- The linker can hand an effect the addresses of its own picture and
  picture info (`_<audio>_Pic`, `_<audio>_Info`), so an effect can redraw
  itself.
- Effect DB: generated slots are listed by range rather than one by one.
- Builds of the animated effects and probes write their Effect Manager
  thumbnail too.

### Animated screens (experimental, MS-70CDR firmware 2.10 only)
An effect can now animate its own pedal screen on stock firmware: no
firmware flash. Research and hardware results:
[docs/pedal-research/animated-covers.md](docs/pedal-research/animated-covers.md),
with thanks to Alexander Lim (Leemuzhko) for the SDK notes that pointed the
way.
- **Probes** (`src/hardware_probes/redraw`, `coverrw`, `blink`): proved
  that the screen redraws only on events, that an effect can rewrite its
  own picture, and that an effect can ask for a redraw. All three were run
  on the pedal.
- **zvis kit** (`src/custom/common/`): draw a 128×64 frame, encode it a
  little per audio block, show it at ~30 fps, only while the effect's card is
  on screen. Firmware addresses are checked first; on any other firmware the
  effect is just a pass-through.
- **Scope** (FXID 414): a live oscilloscope of the input. Works on the pedal.
- **Ring** (FXID 415): a rotating ring that follows the signal. Works on
  the pedal.
- **Skull** (FXID 416): a shaded 3D skull, full screen, that opens its jaw,
  lights its eyes and shakes with the signal, and explodes at the loudest
  peaks (Boom knob). Works on the pedal.

These three are built locally and are not in `dist/` yet.

### Docs and graphics
- The README's main image is redrawn in the Skull's 1-bit dither
  (`graphics/14-console-dithered.png`, made by
  `graphics/make_dithered_hero.py`; 36 KB instead of 2.9 MB).
- NAM Loader guide, NAM template docs and the reserved effect ID list
  updated for 216 slots, names and icons.
- This changelog.

## 2026-10-14
- MS-50G firmware 3.10 confirmed: NAM (full rate and Eco), CabIR and the
  Patch Editor's writes all work (#5).
- Effect DB decodes the pedal's "Hz" glyph, so Graphic EQ is recognised.

## 2026-10-12
- Loader caps on-pedal names at 9 characters (`NAM-`/`CAB-` + 5 letters).
- Pedal research notes for every other Zoom pedal people ask about;
  MS-100BT works through third-party Bluetooth tools.

## 2026-10-09
- Docs: recovery button combo, "Before you start", MIDISTOMP and the
  Matujuice effects pack.
- README: the Four series (G1/B1/A1 Four) is ZD2 and not supported; related
  projects (MS StompCtrl).

## 2026-10-06
- **NAM + Cab Loader**: 16 capture and 16 cab slots, Eco mode, DSP meter.
- NAM beta: tester note, freeze recovery guide, report form.
- Patch Editor: opt-in experimental writes for MS-50G and MS-60B.

Earlier history: `git log`.
