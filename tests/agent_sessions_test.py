#!/usr/bin/env python3
"""Synthetic regressions for project filtering, live snapshots and LSP freshness."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from agent_sessions import inventory, opencode, snapshot, within
from agent_lsp import inspect_database


class AgentHistoryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.project = self.root / 'rr'
        self.project.mkdir()

    def test_project_boundary(self):
        self.assertTrue(within(str(self.project / 'native-port'), self.project))
        self.assertFalse(within(str(self.root / 'rr-other'), self.project))

    def test_snapshot_partial_and_malformed(self):
        path = self.root / 'history.jsonl'
        raw = (json.dumps({'type': 'session', 'id': 'parent', 'cwd': str(self.project)}) + '\n' +
               '{invalid}\n' + json.dumps({'type': 'message', 'message': {'role': 'assistant',
               'content': [{'type': 'text', 'text': 'Result'}, {'type': 'image', 'data': 'secret'}]}}) + '\n').encode() + b'{partial'
        path.write_bytes(raw)
        entry, texts = snapshot(path, 'omp', self.project)
        self.assertEqual(entry['sha256'], hashlib.sha256(raw).hexdigest())
        self.assertEqual(entry['partial_tail_bytes'], 8)
        self.assertEqual(entry['malformed'], 1)
        self.assertEqual(texts[0][2], 'Result')

    def test_prompt_mention_is_not_project_membership(self):
        path = self.root / 'history.jsonl'
        path.write_text(json.dumps({'type': 'session', 'cwd': str(self.root / 'other')}) + '\n' +
                        json.dumps({'type': 'message', 'message': {'role': 'user', 'content': str(self.project)}}) + '\n')
        self.assertIsNone(snapshot(path, 'omp', self.project)[0])

    def test_child_discovery_and_exclusion(self):
        folder = self.root / '.omp/agent/sessions/rr/parent'
        folder.mkdir(parents=True)
        (folder / 'child.jsonl').write_text(json.dumps({'type': 'session', 'id': 'child-id',
            'parentSession': 'parent.jsonl', 'cwd': str(self.project)}) + '\n')
        result = inventory(self.root, self.project)
        self.assertEqual(result['providers'], {'omp': 1})
        self.assertEqual(result['sessions'][0]['parent'], 'parent.jsonl')
        self.assertFalse(inventory(self.root, self.project, ['child-id'])['sessions'])

    def test_opencode_project_and_directory_filter(self):
        path = self.root / 'opencode.db'
        with sqlite3.connect(path) as db:
            db.executescript('CREATE TABLE project(id TEXT, worktree TEXT); CREATE TABLE session(id TEXT, project_id TEXT, directory TEXT, title TEXT, parent_id TEXT); CREATE TABLE message(session_id TEXT); CREATE TABLE part(session_id TEXT);')
            db.execute('INSERT INTO project VALUES (?,?)', ('rr-project', str(self.project)))
            db.executemany('INSERT INTO session VALUES (?,?,?,?,?)', [
                ('by-id', 'rr-project', '/elsewhere', 'project session', None),
                ('by-dir', 'global', str(self.project / 'native-port'), 'child', 'by-id'),
                ('other', 'global', str(self.root / 'rr-other'), 'unrelated', None)])
            db.execute('INSERT INTO message VALUES (?)', ('by-dir',))
        digest = path.read_bytes()
        result = opencode(path, self.project)
        self.assertEqual([s['id'] for s in result['sessions']], ['by-id', 'by-dir'])
        self.assertEqual(result['sessions'][1]['counts']['message'], 1)
        self.assertEqual(path.read_bytes(), digest)

    def test_missing_empty_and_stale_lsp_database(self):
        path = self.root / 'compile_commands.json'
        self.assertFalse(inspect_database(path, self.root)['available'])
        path.write_text('[]')
        self.assertFalse(inspect_database(path, self.root)['healthy'])
        source = self.root / 'test.cpp'
        source.write_text('int main(){}')
        path.write_text(json.dumps([{'directory': str(self.root), 'file': 'test.cpp', 'command': 'c++ test.cpp'}]))
        self.assertTrue(inspect_database(path, self.root)['healthy'])
        source.unlink()
        self.assertEqual(len(inspect_database(path, self.root)['missing_sources']), 1)
        config = self.root / 'CMakeLists.txt'
        config.write_text('# updated')
        os.utime(config, ns=(path.stat().st_mtime_ns + 1000000,) * 2)
        self.assertEqual(len(inspect_database(path, self.root)['newer_config']), 1)


if __name__ == '__main__':
    unittest.main()
