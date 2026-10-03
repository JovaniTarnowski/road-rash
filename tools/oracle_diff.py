#!/usr/bin/env python3
"""Side-by-side original / native / difference image for an oracle scene frame (local use only)."""
import sys
from PIL import Image
tsv, frame, out, palette = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
raw = open(palette, 'rb').read()
pal = {i: raw[4*i] << 16 | raw[4*i+1] << 8 | raw[4*i+2] for i in range(256)}
w, h = 640, 480
stem = tsv[:-4]
orig = open(f'{stem}-f{frame}.raw', 'rb').read()
nat = open(f'{stem}-scene{frame}-native.raw', 'rb').read()
img = Image.new('RGB', (w * 3, h))
px = img.load()
for y in range(h):
    for x in range(w):
        a, b = orig[y*w+x], nat[y*w+x]
        ca, cb = pal.get(a, 0), pal.get(b, 0)
        px[x, y] = (ca >> 16, (ca >> 8) & 255, ca & 255)
        px[x + w, y] = (cb >> 16, (cb >> 8) & 255, cb & 255)
        px[x + 2*w, y] = (255, 0, 0) if a != b else (ca >> 18, ((ca >> 8) & 255) >> 2, (ca & 255) >> 2)
img.save(out)
