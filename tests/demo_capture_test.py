#!/usr/bin/env python3
"""Local-disc integration: repeatable pixels/PCM, moving frames, rational audio.

Usage: python3 tests/demo_capture_test.py BUILD/rr-game [ASSETS]
Commercial output lives only in a temporary directory, never test fixtures.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
assets = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else ROOT.parent / 'cdrom/ROADRASH'
if not (assets / 'ROADRASH.EXE').is_file():
    print('SKIP: original disc missing')
    sys.exit(77)
frames, warmup, frame_bytes = 7, 361, 1280 * 960 * 3
with tempfile.TemporaryDirectory(prefix='rr-capture-test-') as directory:
    tmp = Path(directory)
    signatures = []
    for repeat in range(2):
        prefs = tmp / f'prefs-{repeat}'
        prefs.mkdir()
        video, audio = tmp / f'{repeat}.rgb', tmp / f'{repeat}.pcm'
        subprocess.run([str(binary), '--assets', str(assets), '--course', '0', '--render-scale', '1',
            '--capture-video', str(video), '--capture-audio', str(audio), '--capture-frames', str(frames),
            '--capture-warmup', str(warmup)],
            env=dict(os.environ, RR_PREF_OVERRIDE=str(prefs)), check=True, timeout=120)
        pixels, pcm = video.read_bytes(), audio.read_bytes()
        assert len(pixels) == frames * frame_bytes
        assert len(pcm) == ((warmup + frames) * 22050 // 60 - warmup * 22050 // 60) * 4
        assert pixels[:frame_bytes] != pixels[-frame_bytes:], 'Gameplay did not move'
        assert len(set(pixels[:frame_bytes])) > 64, 'Empty/flat render'
        assert any(pcm), 'Silent capture'
        signatures.append((hashlib.sha256(pixels).hexdigest(), hashlib.sha256(pcm).hexdigest()))
        time.sleep(1.1)  # cross the launch-time seed boundary
    assert signatures[0] == signatures[1], f'Non-repeatable pixels/audio: {signatures}'
    for flags in [ ['--capture-video', str(tmp / 'invalid.rgb')],
                   ['--capture-frames', '-1'],
                   ['--capture-frames', '1', '--capture-audio', str(tmp / 'bad.pcm')]]:
        assert subprocess.run([str(binary), *flags], timeout=10).returncode == 2
print('PASS: exact frames/samples, moving gameplay, non-silent PCM, repeated hashes, invalid arguments')
