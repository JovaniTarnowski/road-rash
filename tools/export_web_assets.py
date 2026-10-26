#!/usr/bin/env python3
"""Export the verified disc set to private staging; never add it to Git/images."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


def export(source, destination):
    root = Path(__file__).resolve().parents[1]
    rows = re.findall(r'\{"([^"}]+)",\s*"([0-9a-f]{64})"\}',
                      (root / 'src/reference_hashes.inc').read_text())
    if len(rows) != 341:
        raise ValueError('Unexpected reference table')
    files = {p.relative_to(source).as_posix().upper(): p for p in source.rglob('*') if p.is_file()}
    destination.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, digest in rows:
        data = files[name].read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Disc hash mismatch: {name}')
        target = destination / digest
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            shutil.copyfile(files[name], target)
        manifest.append(dict(path=name, hash=digest, size=len(data)))
    temporary = destination / 'manifest.json.tmp'
    temporary.write_text(json.dumps(dict(version=1, files=manifest)) + '\n')
    temporary.replace(destination / 'manifest.json')
    print(f'Verified {len(manifest)} files, {sum(f["size"] for f in manifest)} bytes: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    export(args.source.resolve(), args.destination.resolve())
