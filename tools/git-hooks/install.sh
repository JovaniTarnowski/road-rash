#!/bin/sh
# Install the committed git hooks into the current repository's git dir.
# Safe to re-run; works for linked worktrees too.
set -eu

toplevel=$(git rev-parse --show-toplevel)
gitdir=$(git rev-parse --git-dir)
case "$gitdir" in
    /*) hooks="$gitdir/hooks" ;;
    *) hooks="$toplevel/$gitdir/hooks" ;;
esac
mkdir -p "$hooks"
ln -sf "$toplevel/tools/git-hooks/pre-commit" "$hooks/pre-commit"
printf 'installed pre-commit hook -> %s\n' "$hooks/pre-commit"
