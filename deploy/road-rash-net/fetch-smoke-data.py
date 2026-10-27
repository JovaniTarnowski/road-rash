#!/usr/bin/env python3
"""Fetch verified hosted data into an ephemeral bot mount, never an image/artifact."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.request

BASE = 'https://road-rash.archit.xyz/game-assets/'
root = Path(__file__).resolve().parents[2]
expected = dict(re.findall(r'\{"([^"}]+)",\s*"([0-9a-f]{64})"\}',
                          (root / 'src/reference_hashes.inc').read_text()))
if len(sys.argv) != 2 or len(expected) != 341:
    raise SystemExit('usage: fetch-smoke-data.py EPHEMERAL_DIRECTORY (341 references required)')
destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
def request(url, timeout):
    return urllib.request.urlopen(urllib.request.Request(
        url, headers={'User-Agent': 'RoadRash-NetSmoke/1.0'}), timeout=timeout)


with request(BASE + 'manifest.json', timeout=30) as response:
    manifest = json.loads(response.read(1024 * 1024))
rows = manifest.get('files', [])
if manifest.get('version') != 1 or len(rows) != len(expected):
    raise SystemExit('invalid hosted manifest')
seen = set()
for row in rows:
    name = row['path']
    if name in seen or expected.get(name) != row['hash'] or not isinstance(row['size'], int) or not 0 < row['size'] <= 128 * 1024 * 1024:
        raise SystemExit('hosted manifest differs from compiled references')
    seen.add(name)


def fetch(row):
    name, digest, size = row['path'], row['hash'], row['size']
    with request(BASE + digest, timeout=90) as response:
        data = response.read(size + 1)
    if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError('hosted data verification failed: ' + name)
    target = destination / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    list(pool.map(fetch, rows))
print('Verified 341 hosted reference files for ephemeral smoke testing.')
