#!/usr/bin/env python3
"""Read the proxy's `<scenario>-palette.raw` SetEntries log (RR_ORACLE_PALETTE=1)."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

SITES = {0x415e6c: 'fade-in', 0x415f4c: 'fade-out', 0x41642d: 'start-half',
         0x416667: 'buffer-482760', 0x442f2f: 'countdown-expiry', 0x443230: 'pump-half',
         0x443350: 'pump-restore'}
SOURCES = {0x4b8030: 'half', 0x4b8490: 'full', 0x482358: 'fade', 0x482760: '482760'}
FIELDS = ('seq', 'frame', 'ms', 'site', 'source', 'start', 'count', 'countdown', 'fade_counter')


def read(path):
    data = Path(path).read_bytes()
    if data[:8] != struct.pack('<II', 0x4c505252, 1):
        raise ValueError('bad palette log header')
    offset, records = 8, []
    while offset < len(data):
        if offset + 36 > len(data):
            raise ValueError('truncated record header')
        row = dict(zip(FIELDS, struct.unpack_from('<9I', data, offset)))
        offset += 36
        size = row['count'] * 4
        if offset + size > len(data):
            raise ValueError('truncated record entries')
        raw = data[offset:offset + size]
        offset += size
        row['rgb'] = [tuple(raw[i:i + 3]) for i in range(0, size, 4)]
        if row['seq'] != len(records):
            raise ValueError('non-contiguous upload sequence')
        records.append(row)
    return records


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    for row in read(p.parse_args().log):
        flat = bytes(c for rgb in row['rgb'] for c in rgb)
        print(json.dumps({k: row[k] for k in FIELDS} | {
            'site_name': SITES.get(row['site'], hex(row['site'])),
            'source_name': SOURCES.get(row['source'], hex(row['source'])),
            'sha1': hashlib.sha1(flat).hexdigest()[:12], 'max': max(flat, default=0)}))


if __name__ == '__main__':
    main()
