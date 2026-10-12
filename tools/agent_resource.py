#!/usr/bin/env python3
"""Run a command with shared-resource ownership and an unmasked exit code."""
import argparse
import json
from pathlib import Path
import subprocess
from agent_common import ResourceClaims, run_logged


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--session', default='resource-command')
    p.add_argument('--resource', type=Path, action='append', required=True)
    p.add_argument('--log', type=Path)
    p.add_argument('command', nargs=argparse.REMAINDER)
    args = p.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        p.error('a command after -- is required')
    try:
        with ResourceClaims().hold(args.session, args.resource):
            if args.log:
                row = run_logged(command, args.log, cwd=Path.cwd())
                print(json.dumps(row))
                code = row['exit_code']
            else:
                code = subprocess.run(command).returncode
        return code if code >= 0 else 128 - code
    except (RuntimeError, OSError) as e:
        p.exit(1, str(e) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
