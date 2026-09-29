#!/usr/bin/env python3
"""Validate every original CRSR container using the compiled native reader."""
import argparse
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('game', type=Path)
args = parser.parse_args()
paths = []
for path in sorted(args.game.rglob('*')):
    if path.is_file():
        with path.open('rb') as stream:
            if stream.read(4) == b'CRSR':
                paths.append(str(path))
if not paths:
    raise SystemExit('No resource containers found')
result = subprocess.run([str(root/'build/rr-inspect'), *paths], capture_output=True, text=True)
if result.returncode:
    raise SystemExit(result.stderr)
(root/'analysis/resources.tsv').write_text(result.stdout)
entries = sum(line.count('\t') == 3 for line in result.stdout.splitlines())
print(f'Validated {len(paths)} containers and {entries} resource directory entries')
