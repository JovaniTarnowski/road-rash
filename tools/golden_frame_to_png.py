#!/usr/bin/env python3
"""Convert local oracle indices to PNG using the user's original palette."""
import argparse
from pathlib import Path
from PIL import Image
parser = argparse.ArgumentParser()
parser.add_argument('raw', type=Path)
parser.add_argument('width', type=int)
parser.add_argument('height', type=int)
parser.add_argument('palette', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
pixels, palette = args.raw.read_bytes(), args.palette.read_bytes()
if args.width < 1 or args.height < 1 or len(pixels) != args.width * args.height or len(palette) != 1024:
    parser.error('Invalid dimensions, frame length or palette')
rgb = bytearray()
for index in pixels:
    rgb.extend(palette[index*4:index*4+3] if index else b'\0\0\0')
Image.frombytes('RGB', (args.width, args.height), bytes(rgb)).save(args.output)
