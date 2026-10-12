#!/usr/bin/env python3
"""Explicit ownership, worktree creation, and durable per-session handoffs."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from agent_common import ROOT, ResourceClaims, git


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    start = sub.add_parser('start')
    start.add_argument('session')
    start.add_argument('--owner-pid', type=int, required=True, help='long-lived terminal/agent owner PID')
    start.add_argument('--files', nargs='+', required=True, help='relative to target worktree; directories supported')
    start.add_argument('--worktree', type=Path, help='create a detached worktree; dirty edits are NOT copied')
    start.add_argument('--ref', default='HEAD')
    start.add_argument('--task', required=True)
    end = sub.add_parser('release')
    end.add_argument('token')
    end.add_argument('--session', required=True)
    reap = sub.add_parser('reap')
    reap.add_argument('token')
    sub.add_parser('list')
    args = p.parse_args()
    claims = ResourceClaims()
    if args.action == 'list':
        print(json.dumps(claims.list(), indent=2))
    elif args.action == 'release':
        claims.release(args.token, args.session)
    elif args.action == 'reap':
        claims.release(args.token, reap=True)
    else:
        if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.session):
            p.error('session must use letters, numbers, hyphens or underscores')
        root = (args.worktree or ROOT).resolve()
        handoff = root / 'docs/agent-handoffs' / (args.session + '.md')
        if handoff.exists():
            p.error('handoff exists; use a unique session name')
        paths = []
        for name in args.files:
            path = (root / name).resolve()
            if path != root and root not in path.parents:
                p.error('file claims must stay inside the target worktree')
            paths.append(path)
        token = claims.acquire(args.session, paths, args.owner_pid)
        try:
            if args.worktree:
                subprocess.run(['git', '-C', str(ROOT), 'worktree', 'add', '--detach', str(root), args.ref], check=True)
            build = root / ('build-agent-' + args.session)
            handoff.parent.mkdir(parents=True, exist_ok=True)
            handoff.write_text(f'# {args.session}\n\nTask: {args.task}\n\nBaseline: {git("rev-parse", "HEAD", root=root)}\n'
                               f'\nWorktree: `{root}`\nBuild directory: `{build}`\nOwner PID: {args.owner_pid}\n'
                               f'Resource token: `{token}`\n\nOwned files:\n\n' +
                               ''.join(f'- `{x}`\n' for x in paths) +
                               '\n## Progress / evidence\n\nPending.\n\n## Checks / remaining work\n\nPending.\n'
                               '\n## Live processes / resources\n\nFile claims above. Record capture/Ghidra processes here.\n')
            print(json.dumps({'token': token, 'worktree': str(root), 'build_directory': str(build),
                              'handoff': str(handoff), 'dirty_edits_copied': False}, indent=2))
        except Exception:
            claims.release(token, args.session)
            raise


if __name__ == '__main__':
    main()
