#!/usr/bin/env python3
"""Install an oracle proxy under the same resource contract used by captures."""
import argparse
from pathlib import Path
import shutil
from agent_common import ROOT, ResourceClaims, sha256
from oracle_capture import reference_hash


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace', type=Path, default=ROOT.parent)
    p.add_argument('--prefix', type=Path)
    p.add_argument('--proxy', type=Path, default=ROOT / 'oracle/build/AWEMAN32.DLL')
    args = p.parse_args()
    prefix = (args.prefix or args.workspace / 'prefix').resolve()
    src = prefix / 'drive_c/ElectronicArts/RoadRash'
    dst = prefix / 'drive_c/ElectronicArts/RoadRashOracle'
    with ResourceClaims().hold('oracle-install', [prefix, args.proxy]):
        if sha256(src / 'ROADRASH.EXE') != reference_hash():
            p.exit(1, 'reference executable mismatch; installation left unchanged\n')
        if not args.proxy.is_file():
            p.exit(1, 'proxy DLL missing; installation left unchanged\n')
        if dst.exists():
            if sha256(dst / 'ROADRASH.EXE') != reference_hash():
                p.exit(1, 'existing oracle executable mismatch; installation left unchanged\n')
        else:
            shutil.copytree(src, dst)
        real = dst / 'AWEMAN_REAL.DLL'
        if not real.exists():
            shutil.copy2(src / 'AWEMAN32.DLL', real)
        temporary = dst / 'AWEMAN32.DLL.installing'
        shutil.copy2(args.proxy, temporary)
        temporary.replace(dst / 'AWEMAN32.DLL')
        (dst / 'Screenshots').mkdir(exist_ok=True)
        print(f'oracle install ready at {dst}; proxy {sha256(args.proxy)}')


if __name__ == '__main__':
    main()
