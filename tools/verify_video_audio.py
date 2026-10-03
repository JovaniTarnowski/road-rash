#!/usr/bin/env python3
"""Compare the native AVI/IMA decoder with FFmpeg; neither is used at runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path):
    with path.open('rb') as source:
        result = hashlib.sha256()
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
        return result.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('videos', type=Path)
    parser.add_argument('--decoder', type=Path, default=Path('build/avi-test'))
    parser.add_argument('--video-decoder', type=Path)
    args = parser.parse_args()
    if not args.videos.is_dir() or not shutil.which('ffmpeg') or not shutil.which('ffprobe'):
        print('SKIP: disc or FFmpeg unavailable')
        return 77
    files = sorted(args.videos.rglob('*.AVI'))
    audio_count = samples = video_frames = 0
    with tempfile.TemporaryDirectory(prefix='rr-ima-') as directory:
        native = Path(directory) / 'native.pcm'
        reference = Path(directory) / 'reference.pcm'
        for video in files:
            result = subprocess.run([str(args.decoder.resolve()), str(video.resolve()), str(native)], check=True, capture_output=True, text=True)
            video_frames += int(result.stdout.split()[0])
            streams = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', str(video)]))['streams']
            audio = [stream for stream in streams if stream['codec_type'] == 'audio']
            if audio:
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(video), '-map', '0:a', '-f', 's16le', str(reference)], check=True)
                if native.stat().st_size != reference.stat().st_size or digest(native) != digest(reference):
                    raise RuntimeError(f'PCM mismatch: {video}')
                audio_count += 1
                samples += native.stat().st_size // 2
            elif native.stat().st_size:
                raise RuntimeError(f'Unexpected native audio: {video}')
            if args.video_decoder:
                subprocess.run([str(args.video_decoder.resolve()), str(video.resolve()), str(native)], check=True, capture_output=True)
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(video), '-map', '0:v', '-f', 'rawvideo', '-pix_fmt', 'rgb24', str(reference)], check=True)
                if native.stat().st_size != reference.stat().st_size or digest(native) != digest(reference):
                    raise RuntimeError(f'RGB mismatch: {video}')
            print(f'PASS {video.relative_to(args.videos)}', flush=True)
    print(f'{len(files)} AVI files; {audio_count} audio streams; {samples} int16 samples match exactly; {video_frames} video frames' + (' match exactly' if args.video_decoder else ' scanned'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
