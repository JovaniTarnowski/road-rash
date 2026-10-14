#!/usr/bin/env python3
"""Explicit ownership, worktree creation, and durable per-session handoffs."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from agent_common import ROOT, ResourceClaims, git, pid_alive


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
    builds = sub.add_parser('builds')
    builds.add_argument('--root', type=Path, default=ROOT)
    builds.add_argument('--prune', action='store_true')
    builds.add_argument('--yes', action='store_true')
    builds.add_argument('--include-recent', action='store_true')
    builds.add_argument('--older-than', type=float, default=3, metavar='DAYS')
    args = p.parse_args()
    claims = ResourceClaims()
    if args.action == 'list':
        print(json.dumps(claims.list(), indent=2))
    elif args.action == 'release':
        claims.release(args.token, args.session)
    elif args.action == 'reap':
        claims.release(args.token, reap=True)
    elif args.action == 'builds':
        sys.exit(builds_command(claims, args))
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





def tree_size(path):
    # Sizes without following symlinks (plan 30 Task 5).
    total = 0
    for entry in os.scandir(path):
        try:
            if entry.is_symlink():
                continue
            if entry.is_dir(follow_symlinks=False):
                total += tree_size(entry.path)
            else:
                total += entry.stat(follow_symlinks=False).st_size
        except OSError:
            continue
    return total


def builds_command(claims, args):
    # Plan 30 Task 5: claim-aware build-tree listing/pruning. Defaults to
    # dry-run: lists only. Deletes only with --prune --yes, only unclaimed
    # trees older than --older-than days, never anything outside build* dirs
    # at the root, never a tree touched in the last 10 minutes (Focus 5)
    # unless --include-recent.
    from agent_common import ROOT as agent_root
    root = args.root.resolve()
    now = time.time()
    live_paths = set()
    for row in claims.list():
        try:
            alive = pid_alive(row['pid'])
        except (KeyError, TypeError, ValueError):
            alive = False
        if alive:
            for f in row.get('files', row.get('paths', [])):
                live_paths.add(str(Path(f).resolve()))
    rows = []
    for child in sorted(root.iterdir()):
        if not child.is_dir() or child.is_symlink():
            continue
        if child.name == 'build' and root == agent_root.resolve() and not args.prune:
            pass  # `build` itself lists like any tree; deleted only if named (never by default)
        if not child.name.startswith('build'):
            continue
        try:
            mtime = child.stat().st_mtime
        except OSError:
            continue
        # A tree is claimed when a live claim covers it or anything under it.
        claimed = any(str(child.resolve()) == p or str(p).startswith(str(child.resolve()) + os.sep)
                      for p in live_paths)
        recent = (now - mtime) < 10 * 60
        old_enough = (now - mtime) >= args.older_than * 86400
        prunable = not claimed and old_enough and (args.include_recent or not recent)
        rows.append({'name': child.name, 'bytes': tree_size(child), 'mtime': mtime,
                     'claimed': claimed, 'recent': recent, 'prunable': prunable})
    print(json.dumps(rows, indent=2))
    if args.prune:
        if not args.yes:
            print('refusing to delete without --yes', file=sys.stderr)
            return 2
        for row in rows:
            if not row['prunable']:
                continue
            if row['name'] == 'build':
                continue  # never the session tree unless named explicitly (not supported)
            shutil.rmtree(root / row['name'], ignore_errors=True)
    return 0

if __name__ == '__main__':
    main()
