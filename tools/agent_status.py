#!/usr/bin/env python3
"""Render current evidence; only explicit gate receipts update verification."""
import argparse
import json
from pathlib import Path
from agent_common import ROOT, ResourceClaims, atomic_json


def render(data):
    lines = ['# Current status and evidence', '',
             'Generated from `agent-status.json`. Historical entries describe their own checkpoints.', '',
             '| Area | Implementation | Verification / remaining acceptance | Evidence |',
             '| --- | --- | --- | --- |']
    for row in data['areas']:
        evidence = ', '.join(f'[{Path(x).name}]({x})' for x in row['evidence'])
        lines.append(f'| {row["area"]} | {row["implementation"]} | {row["verification"]}; {row["remaining"]} | {evidence} |')
    lines += ['', '## Latest gate receipts', '']
    for name, gate in sorted(data.get('gates', {}).items()):
        lines.append(f'- **{name}**: {gate["status"]}; source `{gate["source"]["source_sha256"][:12]}`; '
                     f'commit `{gate["source"]["commit"][:12]}`; receipt `{gate["receipt"]}`.')
    if not data.get('gates'):
        lines.append('No current receipts. Historical test counts are not current verification.')
    return '\n'.join(lines) + '\n'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, default=ROOT / 'docs/agent-status.json')
    p.add_argument('--output', type=Path, default=ROOT / 'docs/CURRENT_STATUS.md')
    p.add_argument('--record-gate', type=Path)
    args = p.parse_args()
    with ResourceClaims().hold('agent-status', [args.manifest, args.output]):
        data = json.loads(args.manifest.read_text())
        if args.record_gate:
            gate = json.loads(args.record_gate.read_text())
            if gate['status'] == 'running':
                p.error('cannot record an unfinished gate')
            data.setdefault('gates', {})[gate['profile']] = {
                'status': gate['status'], 'source': gate['source'],
                'coverage': gate.get('coverage', {}), 'receipt': str(args.record_gate.resolve())}
            atomic_json(args.manifest, data)
        args.output.write_text(render(data))
    print(args.output)


if __name__ == '__main__':
    main()
