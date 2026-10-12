#!/usr/bin/env python3
"""Find existing address evidence before starting another expensive analysis."""
import argparse
import json
from pathlib import Path
import re
from agent_common import ROOT, atomic_json


def index():
    rows = {}
    for path in sorted((ROOT / 'docs').rglob('*.md')):
        if 'agent-review' in path.parts:
            continue
        for number, line in enumerate(path.read_text().splitlines(), 1):
            for address in set(re.findall(r'0x(?:00)?(4[0-9a-fA-F]{5})\b', line)):
                key = '00' + address.lower()
                rows.setdefault(key, {'decompile': None, 'evidence': []})['evidence'].append(
                    {'file': str(path.relative_to(ROOT)), 'line': number, 'text': line.strip()})
    for path in (ROOT / 'analysis/decompiled').glob('*.c'):
        if re.fullmatch(r'[0-9a-fA-F]{8}', path.stem):
            rows.setdefault(path.stem.lower(), {'decompile': None, 'evidence': []})['decompile'] = str(path.relative_to(ROOT))
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('address', nargs='?', help='hex VA, e.g. 0x004349c0')
    p.add_argument('--output', type=Path, default=ROOT / 'analysis/agent-tools/address-index.json')
    args = p.parse_args()
    data = index()
    if args.address:
        key = f'{int(args.address, 16):08x}'
        print(json.dumps(data.get(key, {'decompile': None, 'evidence': []}), indent=2))
    else:
        atomic_json(args.output, data)
        print(f'{len(data)} addresses indexed at {args.output}')


if __name__ == '__main__':
    main()
