#!/usr/bin/env python3
"""Read-only compile database preflight; optional bounded clangd source check."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from agent_common import ROOT


def inspect_database(database, root=ROOT):
    report = {'database': str(database), 'resolved_database': str(database.resolve()),
              'available': database.is_file(), 'missing_sources': [], 'newer_config': [], 'entries': 0}
    if not database.is_file():
        report['error'] = 'missing compile database; configure an owned build with CMAKE_EXPORT_COMPILE_COMMANDS=ON'
        return report
    try:
        entries = json.loads(database.read_text())
        if not isinstance(entries, list) or not entries:
            raise ValueError('expected a nonempty compile command list')
        for entry in entries:
            source = Path(entry['file'])
            if not source.is_absolute():
                directory = Path(entry['directory'])
                if not directory.is_absolute():
                    directory = database.resolve().parent / directory
                source = directory / source
            if not source.is_file():
                report['missing_sources'].append(str(source.resolve()))
        report['entries'] = len(entries)
        stamp = database.stat().st_mtime_ns
        configs = [root / 'CMakeLists.txt', *sorted((root / 'cmake').rglob('*.cmake'))]
        report['newer_config'] = [str(p) for p in configs if p.is_file() and p.stat().st_mtime_ns > stamp]
    except (ValueError, KeyError, TypeError, OSError) as error:
        report['error'] = str(error)
    report['healthy'] = not (report.get('error') or report['missing_sources'] or report['newer_config'])
    report['limitation'] = 'Checks file presence and configuration timestamps; does not prove client loading or exact flag freshness.'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, default=ROOT / 'compile_commands.json')
    parser.add_argument('--check', type=Path, help='source to parse with clangd; no build/configure or client reload')
    args = parser.parse_args()
    report = inspect_database(args.database.resolve())
    code = 0 if report.get('healthy') else 1
    if args.check:
        clangd = shutil.which('clangd')
        if not clangd:
            report['clangd_error'] = 'clangd unavailable'
            code = 1
        else:
            try:
                result = subprocess.run([clangd, '--check=' + str(args.check.resolve()),
                                         '--compile-commands-dir=' + str(args.database.resolve().parent)],
                                        capture_output=True, text=True, timeout=60)
                report['clangd_check'] = {'source': str(args.check.resolve()), 'exit_code': result.returncode,
                                           'output': result.stdout + result.stderr}
                code = code or result.returncode
            except subprocess.TimeoutExpired:
                report['clangd_error'] = 'check exceeded 60 seconds'
                code = 124
    print(json.dumps(report, indent=2))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
