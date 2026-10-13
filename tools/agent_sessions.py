#!/usr/bin/env python3
"""Inventory local project histories; raw text export is explicit and local only."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sqlite3


def within(value, project):
    if not isinstance(value, str) or not value.startswith('/'):
        return False
    path = Path(value).resolve()
    project = project.resolve()
    return path == project or project in path.parents


def record_text(record):
    """Normalize conversational text without exporting tool arguments or blobs."""
    item = record.get('message') or record.get('payload') or record
    if not isinstance(item, dict):
        return None
    role = item.get('role')
    if record.get('type') == 'event_msg' and item.get('type') == 'user_message':
        role = 'user'
        content = item.get('message', '')
    elif record.get('type') in ('compaction', 'summary'):
        role, content = 'summary', record.get('summary', '')
    else:
        content = item.get('content', '')
    if role not in ('user', 'assistant', 'summary'):
        return None
    if isinstance(content, list):
        content = '\n'.join(c.get('text', '') for c in content if isinstance(c, dict)
                            and c.get('type') in ('text', 'input_text', 'output_text'))
    return (role, content) if isinstance(content, str) and content.strip() else None


def snapshot(path, provider, project):
    # Bound the read to the observed file size. Never chase a live append stream.
    with path.open('rb') as stream:
        size = path.stat().st_size
        raw = stream.read(size)
    complete = raw.rfind(b'\n') + 1
    counts, timestamps, texts = Counter(), [], []
    malformed = 0
    metadata, title, matches = {}, '', False
    for line in raw[:complete].splitlines():
        try:
            record = json.loads(line)
            if not isinstance(record, dict):
                raise ValueError('not an object')
        except (ValueError, UnicodeDecodeError):
            malformed += 1
            continue
        counts[record.get('type', 'unknown')] += 1
        payload = record.get('payload')
        if not isinstance(payload, dict):
            payload = {}
        if within(record.get('cwd'), project) or within(payload.get('cwd'), project):
            matches = True
        if record.get('type') in ('session', 'session_meta'):
            metadata = payload if record['type'] == 'session_meta' else record
        if record.get('type') in ('title', 'title_change', 'custom-title'):
            title = record.get('title') or record.get('customTitle') or title
        timestamp = record.get('timestamp')
        if isinstance(timestamp, str):
            timestamps.append(timestamp)
        text = record_text(record)
        if text:
            texts.append((record.get('timestamp'), *text))
    if not matches:
        return None, []
    return {'provider': provider, 'path': str(path), 'id': metadata.get('id') or metadata.get('sessionId') or path.stem,
            'parent': metadata.get('parentSession') or metadata.get('parent_id'),
            'title': title, 'bytes': len(raw), 'complete_bytes': complete,
            'partial_tail_bytes': len(raw) - complete,
            'sha256': hashlib.sha256(raw).hexdigest(), 'records': sum(counts.values()),
            'counts': dict(counts), 'malformed': malformed,
            'first_timestamp': min(timestamps) if timestamps else None,
            'last_timestamp': max(timestamps) if timestamps else None,
            'hash_scope': 'fixed byte prefix; complete records only parsed'}, texts


def opencode(database, project):
    result = {'database': str(database), 'sessions': [], 'legacy_sessions': [], 'status': 'unavailable'}
    try:
        with sqlite3.connect(database.resolve().as_uri() + '?mode=ro', uri=True) as db:
            db.execute('PRAGMA query_only=ON')
            db.execute('BEGIN')  # one consistent WAL-aware snapshot for all queries
            projects = [row[0] for row in db.execute('SELECT id, worktree FROM project')
                        if within(row[1], project)]
            for row in db.execute('SELECT id, project_id, directory, title, parent_id FROM session'):
                if row[1] in projects or within(row[2], project):
                    counts = {table: db.execute(f'SELECT count(*) FROM {table} WHERE session_id=?', (row[0],)).fetchone()[0]
                              for table in ('message', 'part')}
                    result['sessions'].append(dict(id=row[0], project_id=row[1], directory=row[2],
                                                   title=row[3], parent=row[4], counts=counts))
            result.update(status='reviewable' if result['sessions'] else 'no_stored_conversations',
                          project_ids=projects, snapshot='read-only SQLite transaction including WAL')
    except sqlite3.Error as error:
        result.update(status='error' if database.is_file() else 'unavailable', error=str(error))
    legacy = database.parent / 'storage/session'
    result['legacy_store'] = str(legacy)
    result['legacy_parse_errors'] = 0
    for path in sorted(legacy.rglob('*.json')) if legacy.is_dir() else []:
        try:
            record = json.loads(path.read_text())
            if within(record.get('directory'), project) or record.get('projectID') in result.get('project_ids', []):
                result['legacy_sessions'].append({'id': record.get('id'), 'path': str(path),
                    'directory': record.get('directory'), 'title': record.get('title'),
                    'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        except (ValueError, OSError, AttributeError):
            result['legacy_parse_errors'] += 1
    if result['legacy_sessions']:
        result['status'] = 'reviewable'
    return result


def inventory(home, project, excluded=(), text_dir=None):
    sessions, gaps = [], []
    roots = {'codex': [home / '.codex/sessions', home / '.codex/archived_sessions'],
             'claude': [home / '.claude/projects'], 'omp': [home / '.omp/agent/sessions']}
    for provider, directories in roots.items():
        for directory in directories:
            if not directory.is_dir():
                gaps.append({'provider': provider, 'path': str(directory), 'status': 'missing_store'})
                continue
            for path in sorted(directory.rglob('*.jsonl')):
                # Fast filter uses metadata, not a prompt mentioning another project.
                try:
                    with path.open('rb') as stream:
                        head = stream.read(131072)
                    if str(project).encode() not in head:
                        continue
                    entry, texts = snapshot(path, provider, project)
                except OSError as error:
                    gaps.append({'provider': provider, 'path': str(path), 'status': 'read_error', 'error': str(error)})
                    continue
                if not entry or any(value in entry['id'] or value in str(path) for value in excluded):
                    continue
                sessions.append(entry)
                if text_dir:
                    text_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
                    # Hash the absolute path: provider child IDs/stems need not be unique.
                    name = provider + '-' + hashlib.sha256(str(path).encode()).hexdigest()[:16] + '.txt'
                    export = text_dir / name
                    export.touch(mode=0o600)
                    export.write_text(str(path) + '\n\n' + '\n\n'.join(
                        f'[{timestamp}] {role}\n{text}' for timestamp, role, text in texts))
                    entry['local_text_export'] = str(text_dir / name)
    return {'generated_at': datetime.now(timezone.utc).isoformat(), 'project': str(project),
            'scope': 'local retained histories with project cwd metadata; excludes remote/deleted history; attachments not inspected',
            'excluded_ids': list(excluded), 'providers': dict(Counter(s['provider'] for s in sessions)),
            'records': sum(s['records'] for s in sessions), 'sessions': sessions, 'gaps': gaps,
            'opencode': opencode(home / '.local/share/opencode/opencode.db', project)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, default=Path.home())
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--exclude-id', action='append', default=[])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--local-text-dir', type=Path, help='private review excerpts; must be outside the Git repository')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    if args.local_text_dir and (args.local_text_dir.resolve() == repo or repo in args.local_text_dir.resolve().parents):
        parser.error('raw conversation text must be exported outside the Git repository')
    result = inventory(args.home.resolve(), args.project.resolve(), args.exclude_id, args.local_text_dir)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result[key] for key in ('providers', 'records', 'gaps')}))
    print('OpenCode:', result['opencode']['status'])


if __name__ == '__main__':
    main()
