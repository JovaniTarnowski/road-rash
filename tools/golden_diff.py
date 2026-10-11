#!/usr/bin/env python3
"""Compare two rr-oracle golden files field by field; print the first difference."""
import sys

def load(path):
    frames, cur = [], None
    with open(path) as f:
        if f.readline().rstrip('\r\n') != '# rr-oracle 1':
            sys.exit(f'{path}: bad magic')
        ended = False
        for line in f:
            line = line.rstrip('\r\n')
            if not line or line.startswith('#'):
                continue
            if line == 'end':
                ended = True
                continue
            k, v = line.split('\t', 1)
            if k == 'frame':
                cur = {}
                frames.append(cur)
            elif cur is not None:
                cur[k] = int(v)
    if not ended:
        sys.exit(f'{path}: truncated capture (no end marker)')
    return frames

a, b = load(sys.argv[1]), load(sys.argv[2])
prefixes = tuple(sys.argv[3:]) or ('rider.', 'car.', 'rng', 'tick')
for i, fa in enumerate(a):
    if 'tick' in fa and fa['tick'] != a[0].get('tick', 0) + i:
        sys.exit(f'{sys.argv[1]}: tick not contiguous at frame {i}')
for i, (fa, fb) in enumerate(zip(a, b)):
    for k in sorted(set(fa) | set(fb)):
        if k.startswith(prefixes) and fa.get(k) != fb.get(k):
            sys.exit(f'frame {i} {k}: {fa.get(k)} != {fb.get(k)}')
if len(a) != len(b):
    sys.exit(f'frame counts differ: {len(a)} != {len(b)}')
print(f'{len(a)} frames identical')
