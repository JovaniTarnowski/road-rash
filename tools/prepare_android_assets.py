#!/usr/bin/env python3
"""Copy only original files needed by the current research viewer into the APK."""
import argparse
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('game', type=Path)
args = parser.parse_args()
target = root/'android/app/src/main/assets/game'
files = ['DATA/BIKESPEC.RSC'] + ['IMAGES/'+name+'.RRI' for name in
    ['SPLASH', 'MAIN', 'OLLEYS', 'PANZER', 'STREET', 'CREDITS']]
files += ['DATA/PALETTE.RAW', 'IMAGES/BIGMETER.BOB', 'IMAGES/HORIZONS/NAPA.BOB', 'IMAGES/HORIZONS/CITY.BOB']
# Course geometry, plus the executable for its static projection table (0x0046f098).
files += ['DATA/BIKERS/HIBOB.DAT']
files += ['ROADRASH.EXE'] + ['DATA/COURSES/'+name+'.CRS' for name in ['NAPA', 'CITY', 'HIWAY', 'CANYN', 'MEDLY']]
for relative in files:
    out = target/relative
    out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.game/relative, out)
print(f'Copied {len(files)} original assets for local Android build')
