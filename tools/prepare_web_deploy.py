#!/usr/bin/env python3
"""Validate the asset-free bundle, then stage Vercel Build Output API files."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

root = Path(__file__).resolve().parents[1]
dist = Path(sys.argv[1]).resolve()
allowed = {'index.html', 'app.js', 'style.css', 'rr-game.js', 'rr-game.wasm', 'references.json', 'LICENSES.txt'}
files = {p.name for p in dist.iterdir() if p.is_file()}
if files != allowed or any(p.is_dir() for p in dist.iterdir()):
    raise SystemExit(f'Unexpected web bundle contents: {files ^ allowed}')
hashes = set(re.findall(r'[0-9a-f]{64}', (root / 'src/reference_hashes.inc').read_text()))
for name in allowed:
    if hashlib.sha256((dist / name).read_bytes()).hexdigest() in hashes:
        raise SystemExit(f'Game asset found in web bundle: {name}')
output = root / '.vercel/output'
static = output / 'static'
# Only this generated deployment directory is replaced. Project linkage stays intact.
if static.exists():
    shutil.rmtree(static)
shutil.copytree(dist, static)
config = {'version': 3, 'routes': [
    {'src': '/(.*)', 'headers': {
        'Cache-Control': 'public, max-age=0, must-revalidate',
        'X-Content-Type-Options': 'nosniff',
        'Referrer-Policy': 'same-origin',
        'Content-Security-Policy': "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'self'; frame-ancestors 'self'"
    }, 'continue': True},
    {'handle': 'filesystem'}
]}
(output / 'config.json').write_text(json.dumps(config, indent=2) + '\n')
print(f'Validated {len(allowed)} code/metadata files; no game assets. Vercel output: {output}')
