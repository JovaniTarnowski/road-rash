#!/usr/bin/env python3
"""Read-only capability report. Missing capabilities do not trigger installation."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from agent_common import ROOT, ResourceClaims, git, pid_alive, sha256
from oracle_capture import reference_hash


def command(name, args=('--version',)):
    path = shutil.which(name)
    if not path:
        return {'available': False}
    try:
        result = subprocess.run([path, *args], capture_output=True, text=True, timeout=10)
        return {'available': result.returncode == 0, 'path': path,
                'version': (result.stdout + result.stderr).splitlines()[:2], 'exit_code': result.returncode}
    except (OSError, subprocess.TimeoutExpired) as e:
        return {'available': False, 'path': path, 'error': str(e)}


def doctor(adb_serial=None):
    tools = {name: command(name) for name in ['cmake', 'ctest', 'clang++', 'python3', 'node', 'npx',
                                             'i686-w64-mingw32-gcc', 'clang-tidy']}
    java = Path(os.environ.get('JAVA_HOME', '/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home'))
    ghidra = Path(os.environ.get('GHIDRA_HOME', '/opt/homebrew/opt/ghidra/libexec'))
    sdk = ROOT / '.tools/android-sdk'
    properties = ROOT / 'android/local.properties'
    if properties.exists():
        for line in properties.read_text().splitlines():
            if line.startswith('sdk.dir='):
                sdk = Path(line.split('=', 1)[1])
    sdk = Path(os.environ.get('ANDROID_HOME', str(sdk)))
    assets = ROOT.parent / 'cdrom/ROADRASH/ROADRASH.EXE'
    exe = {'path': str(assets), 'available': assets.is_file()}
    if assets.is_file():
        exe.update(sha256=sha256(assets), reference_matches=sha256(assets) == reference_hash())
    wine = Path(os.environ.get('RR_WINE', str(Path.home() / 'Applications/Wine Staging.app/Contents/Resources/wine/bin/wine')))
    # adb devices may start a server; inspect only when it is already running.
    ps = subprocess.run(['ps', '-axo', 'command'], capture_output=True, text=True)
    server_running = any('adb' in s and 'fork-server server' in s for s in ps.stdout.splitlines())
    adb = {'path': shutil.which('adb'), 'selected_serial': adb_serial or os.environ.get('ANDROID_SERIAL'),
           'server_running': server_running, 'devices': [], 'hardware_validated': False}
    if adb['path'] and server_running:
        devices = subprocess.run([adb['path'], 'devices', '-l'], capture_output=True, text=True, timeout=10)
        adb['devices'] = devices.stdout.splitlines()[1:]
    instructions = {}
    for path in [ROOT.parent / 'AGENTS.md', ROOT.parent / 'CLAUDE.md', ROOT / 'AGENTS.md', ROOT / 'CLAUDE.md']:
        instructions[str(path)] = {'exists': path.is_file(), 'sha256': sha256(path) if path.is_file() else None}
    return {'git_root': git('rev-parse', '--show-toplevel'), 'dirty_files': git('status', '--short').splitlines(),
            'claims': [dict(r, owner_alive=pid_alive(r['pid'])) for r in ResourceClaims().list()],
            'tools': tools, 'java_home': {'path': str(java), 'available': (java / 'bin/java').is_file()},
            'sdl3': command('pkg-config', ('--modversion', 'sdl3')),
            'android_sdk': {'path': str(sdk), 'available': sdk.is_dir(),
                            'ndk_28_2': (sdk / 'ndk/28.2.13676358').is_dir()},
            'ghidra': {'path': str(ghidra), 'available': (ghidra / 'support/analyzeHeadless').is_file()},
            'wine': {'path': str(wine), 'available': wine.is_file()}, 'reference_exe': exe,
            'rea': {'pinned_version': '6.4.0', 'runtime': 'Node 22.19+/24.11+/26+ per docs/TOOLING.md',
                    'invocation': 'npx --yes rea-agents@6.4.0', 'executed': False},
            'adb': adb, 'instruction_files': instructions,
            'instruction_loading': 'File/pointer presence only; actual per-client loading must be checked in that client.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb-serial')
    args = p.parse_args()
    print(json.dumps(doctor(args.adb_serial), indent=2))


if __name__ == '__main__':
    main()
