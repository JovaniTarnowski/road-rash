#!/usr/bin/env python3
"""Generate stable source/config identity for binaries and packaging."""
import argparse
import json
from pathlib import Path
from agent_common import ROOT, atomic_json, source_identity


def identity(config, compiler):
    return dict(source_identity(), configuration=config, compiler=compiler)


def write_header(path, info):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(info, sort_keys=True)
    content = '#pragma once\nnamespace rr { inline constexpr const char* build_info_json = ' + json.dumps(payload) + '; }\n'
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', default='Debug')
    p.add_argument('--compiler', default='unknown')
    p.add_argument('--header', type=Path)
    p.add_argument('--manifest', type=Path)
    p.add_argument('--check', type=Path, help='verify manifest source matches current tree')
    args = p.parse_args()
    info = identity(args.config, args.compiler)
    if args.check:
        saved = json.loads(args.check.read_text())
        for key in ['commit', 'source_sha256']:
            if saved[key] != info[key]:
                p.exit(1, f'stale build: {key} differs; rebuild\n')
        print('PASS source identity')
    else:
        if args.header:
            write_header(args.header, info)
        if args.manifest:
            atomic_json(args.manifest, info)
        print(json.dumps(info, sort_keys=True))


if __name__ == '__main__':
    main()
