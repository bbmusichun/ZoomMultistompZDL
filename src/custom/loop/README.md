# Loop — a looper for stock firmware

`Loop.ZDL` (FXID 467, Delay group) records a loop, plays it, and lets you
overdub, undo and redo, all from the footswitch. It runs on the stock
firmware, with no MIDISTOMP needed. Its screen (state, time and waveform) needs
MS-70CDR 2.10. Tested on the pedal on 2026-10-19.

Put Loop last in the patch, so it records everything before it.

## The footswitch

The footswitch is the looper's button. Each press flips the effect's on/off
LED, but **the loop plays either way**, so ignore the LED.

| You're in | Single press | Double press (within ~0.45 s) |
|---|---|---|
| READY (empty) | start recording | cancel the recording |
| REC | close the loop and play it from the top | close the loop and stop |
| PLAY | start an overdub | stop (and drop the overdub the first press began) |
| DUB | back to PLAY: the overdub ends | stop, keeping the overdub |
| STOP | play from the top | **clear the loop** (back to READY) |

- **The first press acts at once**, so your timing is exact. A second press
  inside the window turns the pair into a double press.
- **Loop length:** your second press sets it. If you reach the memory limit,
  the loop closes and starts playing by itself.
- **Short presses:** a loop under 0.15 s counts as a mistaken press and is
  thrown away.

**To clear the loop:** double press to stop it, then double press again while
it's stopped. The first press of the clearing pair briefly starts the loop
again before the second one clears it.

An on/off message over MIDI, or the on/off button in Patch Editor, counts as a
press too.

## Knobs

| Page | Knob | What it does |
|---|---|---|
| 1 | **Loop** | Loop playback level. |
| 1 | **Dry** | Your live signal. |
| 1 | **Fade** | How much of the loop survives each overdub lap: 100 keeps everything; lower values let older layers fade away, like a delay's feedback. |
| 2 | **Undo** | Flip it either way to undo the last overdub, then flip it again to redo. Flipping it during an overdub throws that overdub away. |
| 2 | **Long** | 1 gives longer loops (about 23 s instead of about 5 s); see below. It only switches over while the looper is empty, so set it before recording. |

## The screen

- **Top left:** the state: READY, REC, PLAY, DUB or STOP.
- **Top right:** the time. READY shows how long you can record, REC shows the
  time recorded so far, and otherwise it shows the loop length.
- **Middle:** the loop's waveform, with a playhead. While you record, the
  waveform fills in from the left.
- **Tags:**
  - **UNDO:** there is an overdub you can undo.
  - **LONG:** the extra memory is in use.
  - **NO LONG:** you turned Long on, but it isn't available (see below).

## Length and sound

- **Sound:** a mono loop (left and right mixed), played into both outputs.
  It's stored as 12-bit block floating point with headroom up to 8× full
  scale, so stacked overdubs don't clip.
- **Normal length:** about 5 s, using only the memory the pedal gives the
  effect (~690 KB). Half of it holds the second copy for undo.
- **Long:** about 23 s. This adds 2.7 MB of the MS-70CDR 2.10 firmware's RAM
  that [MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp)'s RAM test
  found unused (their issue #19):

  | Region | Address | Size (bytes) |
  |---|---|---|
  | 1 | `0xC0102000` | 614,400 |
  | 2 | `0xC0198000` | 425,984 |
  | 3 | `0xC0636980` | 1,677,952 |

  Loop uses these regions only when all of these hold:
  - the firmware is MS-70CDR 2.10;
  - MIDISTOMP isn't installed (its own looper uses the same RAM);
  - none of the regions overlaps Loop's own memory.

  Otherwise the screen shows NO LONG and Loop keeps the normal length.
  Long is experimental: if anything misbehaves, set Long back to 0.
- **When the loop is lost:** when you change patches or turn the pedal off.
- **Auto Save:** with Auto Save on, the pedal also saves the effect's on/off
  state on each press, as it does whenever you switch any effect.

## How it works

- **The footswitch:** the pedal keeps running an effect while it's switched
  off. It only hands the effect an on/off value, and the effect decides what
  to output. Loop treats every flip of that value as a button press, and its
  output doesn't depend on it.
- **Storage:** each audio callback is one block of 8 samples, stored as eight
  12-bit mantissas and a shared shift. Four blocks make a 52-byte record.
- **Undo without copying:** there are two copies of the loop. An overdub
  reads the current loop, adds your playing, and writes the result into the
  other copy for one full lap. If you're still overdubbing when the lap ends,
  further laps build up in that copy. When the lap completes, the new copy
  becomes the loop and the old one is the undo. Undo and redo just switch
  which copy plays.
- **The screen:** drawn with the zvis kit (`../common/zvis.h`), like Scope and
  Skull. That kit is how an effect animates its own screen on stock firmware;
  see [animated-covers.md](../../../docs/pedal-research/animated-covers.md).
- **DSP:** at most ~2.5k cycles per callback in the emulator, light.

## Building and checking

```bash
python3 src/custom/loop/build.py
```

```bash
python3 src/custom/loop/check.py
```

```bash
python3 src/custom/loop/check.py --long
```

- `build.py` writes `dist/Loop.ZDL` and needs the TI toolchain.
- `check.py` runs `loop.c` on the host through a scripted session (record,
  play, overdub, undo, redo, stop, clear, a take past the memory limit) and
  compares every output sample with the loop it should hold. It needs numpy.
- `check.py --long` runs the same checks with the Long regions pointed at host
  buffers, including a 12 s take across all of them.
- In the emulator, `NAMCHECK_PRESSES=<block,…>` presses the footswitch through
  the pedal's own on/off handler (`tools/emulator/src/bin/namcheck.rs`).
