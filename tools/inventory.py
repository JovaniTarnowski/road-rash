#!/usr/bin/env python3
"""Read-only asset inventory and PE import report. Standard library only."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def pe_report(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, count, timestamp, _, _, optsize, flags = struct.unpack_from('<HHIIIHH', data, pe+4)
    opt = pe+24
    if struct.unpack_from('<H', data, opt)[0] != 0x10b:
        raise ValueError('Expected PE32')
    sections = []
    for i in range(count):
        pos = opt+optsize+40*i
        name = data[pos:pos+8].rstrip(b'\0').decode('ascii')
        vsize, va, size, offset = struct.unpack_from('<IIII', data, pos+8)
        sections.append(dict(name=name, rva=va, virtual_size=vsize, file_size=size, offset=offset))
    def at(rva):
        for s in sections:
            if s['rva'] <= rva < s['rva']+s['file_size']:
                return s['offset']+rva-s['rva']
        raise ValueError(f'Unmapped RVA {rva:x}')
    def string(pos):
        return data[pos:data.index(b'\0', pos)].decode('ascii')
    imports = {}
    pos = at(struct.unpack_from('<I', data, opt+104)[0])
    while any(data[pos:pos+20]):
        lookup, _, _, name, iat = struct.unpack_from('<IIIII', data, pos)
        dll = string(at(name)); imports[dll] = []
        thunk = at(lookup or iat)
        while (entry := struct.unpack_from('<I', data, thunk)[0]):
            imports[dll].append(f'ordinal:{entry & 0xffff}' if entry & 0x80000000 else string(at(entry)+2))
            thunk += 4
        pos += 20
    return dict(machine=hex(machine), timestamp=timestamp, characteristics=hex(flags),
                image_base=hex(struct.unpack_from('<I', data, opt+28)[0]),
                entry_rva=hex(struct.unpack_from('<I', data, opt+16)[0]), sections=sections, imports=imports)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', type=Path, default=Path('analysis/inventory.json'))
    args = parser.parse_args()
    files = []
    for p in sorted(args.game.rglob('*')):
        if not p.is_file():
            continue
        data = p.read_bytes()
        kind = 'unknown'
        for magic, label in [(b'\xff\xd8\xff', 'JPEG'), (b'CRSR', 'resource-container'),
                             (b'RIFF', 'RIFF'), (b'MZ', 'Windows-executable')]:
            if data.startswith(magic):
                kind = label
                break
        files.append(dict(path=p.relative_to(args.game).as_posix(), size=len(data),
                          sha256=hashlib.sha256(data).hexdigest(), format=kind))
    report = dict(files=files, executable=pe_report((args.game/'ROADRASH.EXE').read_bytes()))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print(f'Inventoried {len(files)} files into {args.out}')


if __name__ == '__main__':
    main()
