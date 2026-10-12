#!/usr/bin/env python3
"""Verify a self-contained ARM64 app; this does not establish gameplay fidelity."""
import argparse
import json
from pathlib import Path
import plistlib
import subprocess
from agent_common import source_identity


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('app', type=Path)
    args = parser.parse_args()
    app = args.app.resolve()
    with (app / 'Contents/Info.plist').open('rb') as source:
        info = plistlib.load(source)
    # The bundle must have been built from the current HEAD (plan 11a Task 4):
    # a stale app cannot be shipped as "the current game".
    repo = Path(__file__).resolve().parent.parent
    expected = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', '--short', 'HEAD'], text=True).strip()
    stamp = info.get('RRBuildCommit')
    if stamp != expected:
        raise RuntimeError(f'App build commit {stamp!r} != HEAD {expected!r}; rebuild with tools/build_macos_app.sh')
    executable = app / 'Contents/MacOS' / info['CFBundleExecutable']
    provenance = json.loads((app / 'Contents/Resources/build-info.json').read_text())
    current = source_identity()
    for key in ['commit', 'source_sha256']:
        if provenance[key] != current[key]:
            raise RuntimeError(f'Stale source identity: {key}; rebuild the app')
    if provenance['configuration'] != 'Release':
        raise RuntimeError('App is not a Release build')
    embedded = json.loads(subprocess.check_output([str(executable), '--build-info'], text=True))
    if embedded != provenance:
        raise RuntimeError('Binary build identity differs from packaged manifest')
    architectures = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).strip()
    if architectures != 'arm64':
        raise RuntimeError(f'Unexpected architecture: {architectures}')
    dependencies = subprocess.check_output(['otool', '-L', str(executable)], text=True).splitlines()[1:]
    for entry in dependencies:
        library = entry.strip().split(' (', 1)[0]
        if not library.startswith(('/System/Library/', '/usr/lib/')):
            raise RuntimeError(f'External runtime dependency: {library}')
    for path in app.rglob('*'):
        if path.suffix.upper() in {'.EXE', '.RSC', '.RRI', '.BOB', '.CRS', '.DAT', '.WAV', '.RRA', '.AVI', '.ISO', '.MID', '.SBK'}:
            raise RuntimeError(f'Unexpected bundled game data: {path}')
    for name in ['SDL-LICENSE.txt', 'LICENSE', 'README.txt']:
        if not (app / 'Contents/Resources' / name).is_file():
            raise RuntimeError(f'Missing licence/notice: {name}')
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    print(f'PASS: ARM64, {len(dependencies)} system libraries/frameworks, valid ad-hoc signature, no game data, built from {stamp}')


if __name__ == '__main__':
    main()
