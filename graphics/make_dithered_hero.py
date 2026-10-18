#!/usr/bin/env python3
"""The README hero in the Skull effect's look: 1-bit, 8x8 ordered dither.

Smooth tones (grime and rust median-filtered away) are dithered at half
resolution with src/custom/skull/skull3d.py's tone and Bayer dither; edges and
lettering, found at full resolution against a local mean, are laid on top in
solid black and white so the text stays legible.
    python3 graphics/make_dithered_hero.py
"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageOps, ImageFilter

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'src/custom/skull'))
import skull3d as S  # noqa: E402

SRC, OUT = HERE / '13-ivory-console-film.png', HERE / '14-console-dithered.png'
PX, GAMMA, K_DARK, K_LIGHT, MEDIAN = 2, 2.0, 0.09, 0.11, 9


def pool(mask, px):
    """Downscale a mask: a cell is set when at least half its pixels are."""
    h, w = mask.shape[0] // px, mask.shape[1] // px
    return mask[:h * px, :w * px].reshape(h, px, w, px).mean((1, 3)) >= 0.5


def main():
    src = Image.open(SRC).convert('L')
    w, h = src.size
    tone = src.filter(ImageFilter.MedianFilter(MEDIAN)).filter(ImageFilter.MedianFilter(MEDIAN))
    tone = ImageOps.autocontrast(tone.resize((w // PX, h // PX), Image.LANCZOS), cutoff=0.5)
    out = S.dither(S.tone((np.asarray(tone, float) / 255) ** GAMMA, 0.03, 0.92), 'bayer')
    det = src.filter(ImageFilter.MedianFilter(3))
    d = np.asarray(det, float) / 255 - np.asarray(det.filter(ImageFilter.GaussianBlur(5)), float) / 255
    out = (out | pool(d > K_LIGHT, PX)) & ~pool(d < -K_DARK, PX)
    Image.fromarray((out * 255).astype(np.uint8)).resize((w, h), Image.NEAREST).convert('1').save(OUT, optimize=True)
    print(f'-> {OUT}')


if __name__ == '__main__':
    main()
