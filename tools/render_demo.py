#!/usr/bin/env python3
"""Render native gameplay offline to H.264/AAC MP4 (default: five courses, 120s).

Run from anywhere. Requires the original local disc assets and ffmpeg/ffprobe.
No screen recorder, realtime clock, audio device, or commercial soundtrack.
"""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    subprocess.run([str(x) for x in command], check=True, **kwargs)


def probe(path):
    return json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-count_frames', '-show_streams', '-show_format',
        '-of', 'json', str(path)]))


def verify(path, frames):
    info = probe(path)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    audio = next(s for s in info['streams'] if s['codec_type'] == 'audio')
    if (video['codec_name'], video['width'], video['height'], video['pix_fmt'],
        video['r_frame_rate'], int(video['nb_read_frames'])) != ('h264', 1280, 960, 'yuv420p', '60/1', frames):
        raise RuntimeError(f'Unexpected video stream: {video}')
    if audio['codec_name'] != 'aac' or abs(float(info['format']['duration']) - frames / 60) > .1:
        raise RuntimeError('Unexpected audio codec or duration')
    return info


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets', type=Path, default=ROOT.parent / 'cdrom/ROADRASH')
    p.add_argument('--build', type=Path, default=ROOT / 'build-demo-video')
    p.add_argument('--no-build', action='store_true')
    p.add_argument('--seconds', type=float, default=120, help='total duration, split equally across courses')
    p.add_argument('--courses', type=int, nargs='+', default=[0, 1, 2, 3, 4], choices=range(5))
    p.add_argument('--warmup', type=int, default=360, help='simulation ticks before each clip')
    p.add_argument('--level', type=int, default=1, choices=range(1, 6))
    p.add_argument('--bike', type=int, default=0)
    p.add_argument('--render-scale', type=int, default=2, choices=range(1, 5))
    p.add_argument('--crf', type=int, default=18, choices=range(0, 52))
    p.add_argument('--out', type=Path, default=ROOT / 'analysis/demo-video' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    a = p.parse_args()
    if not math.isfinite(a.seconds) or a.seconds <= 0 or a.warmup < 0:
        p.error('seconds must be positive and warmup nonnegative')
    total = round(a.seconds * 60)
    if total < len(a.courses):
        p.error('duration must provide at least one frame per course')
    for tool in ['ffmpeg', 'ffprobe']:
        if not shutil.which(tool):
            p.error(f'{tool} is required')
    a.assets, a.build, a.out = a.assets.resolve(), a.build.resolve(), a.out.resolve()
    if not (a.assets / 'ROADRASH.EXE').is_file():
        p.error(f'Original assets missing at {a.assets}')
    a.out.mkdir(parents=True, exist_ok=False)
    if not a.no_build:
        with (a.out / 'build.log').open('w') as log:
            run(['cmake', '-S', ROOT, '-B', a.build, '-DCMAKE_BUILD_TYPE=Release'], stdout=log, stderr=log)
            run(['cmake', '--build', a.build, '--parallel', '--target', 'rr-game'], stdout=log, stderr=log)
    binary = a.build / 'rr-game'
    receipt = {'build': json.loads(subprocess.check_output([str(binary), '--build-info'])),
               'fps': 60, 'resolution': [1280, 960], 'frames': total,
               'driver': 'native autopilot', 'audio': 'native effects/engines, no soundtrack',
               'render_scale': a.render_scale, 'clips': []}
    clips = []
    for i, course in enumerate(a.courses):
        frames = total // len(a.courses) + (i < total % len(a.courses))
        stem = f'{i + 1:02d}-course-{course}'
        clip = a.out / f'{stem}.mp4'
        print(f'Rendering {stem}: {frames / 60:.2f}s ({frames} frames)', flush=True)
        with tempfile.TemporaryDirectory(prefix='rr-demo-') as tmp:
            tmp = Path(tmp)
            pcm, encoded = tmp / 'audio.pcm', tmp / 'video.mp4'
            (tmp / 'prefs').mkdir()
            env = dict(os.environ, RR_PREF_OVERRIDE=str(tmp / 'prefs'))
            game_cmd = [str(binary), '--assets', str(a.assets), '--course', str(course),
                        '--bike', str(a.bike), '--level', str(a.level),
                        '--render-scale', str(a.render_scale),
                        '--capture-audio', str(pcm), '--capture-frames', str(frames),
                        '--capture-warmup', str(a.warmup)]
            with (a.out / f'{stem}-encoder.log').open('w') as log:
                encoder = subprocess.Popen(['ffmpeg', '-v', 'warning', '-y', '-f', 'rawvideo',
                    '-pixel_format', 'rgb24', '-video_size', '1280x960', '-framerate', '60',
                    '-i', 'pipe:0', '-an', '-c:v', 'libx264', '-preset', 'fast', '-crf', str(a.crf),
                    '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(encoded)], stdin=subprocess.PIPE, stderr=log)
                try:
                    with (a.out / f'{stem}-engine.log').open('w') as game_log:
                        fd = encoder.stdin.fileno()
                        game_cmd += ['--capture-video', f'/dev/fd/{fd}']
                        run(game_cmd, env=env, pass_fds=(fd,), stdout=game_log, stderr=game_log)
                    encoder.stdin.close()
                    if encoder.wait(timeout=120) != 0:
                        raise RuntimeError(f'Encoder failed; see {stem}-encoder.log')
                finally:
                    if encoder.poll() is None:
                        encoder.kill()
                        encoder.wait()
            expected_audio = ((a.warmup + frames) * 22050 // 60 - a.warmup * 22050 // 60) * 4
            if pcm.stat().st_size != expected_audio:
                raise RuntimeError('Audio sample count mismatch')
            run(['ffmpeg', '-v', 'error', '-y', '-i', encoded, '-f', 's16le', '-ar', '22050',
                 '-ac', '2', '-i', pcm, '-map', '0:v:0', '-map', '1:a:0', '-c:v', 'copy',
                 '-c:a', 'aac', '-b:a', '160k', '-ar', '48000', '-movflags', '+faststart', clip])
            verify(clip, frames)
        clips.append(clip)
        receipt['clips'].append({'course': course, 'frames': frames, 'path': clip.name,
                                 'warmup_ticks': a.warmup, 'command': game_cmd})
        (a.out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    final = a.out / 'road-rash-native-demo.mp4'
    # Decode AAC segments before joining to remove independent encoder padding.
    inputs = [item for clip in clips for item in ['-i', str(clip)]]
    filters = ''.join(
        f'[{i}:v:0]setpts=N/(60*TB)[v{i}];'
        f'[{i}:a:0]atrim=duration={row["frames"] / 60},asetpts=PTS-STARTPTS[a{i}];'
        for i, row in enumerate(receipt['clips']))
    filters += ''.join(f'[v{i}][a{i}]' for i in range(len(clips)))
    run(['ffmpeg', '-v', 'error', '-y', *inputs, '-filter_complex',
         filters + f'concat=n={len(clips)}:v=1:a=1[vc][a];[vc]fps=60[v]', '-map', '[v]', '-map', '[a]',
         '-c:v', 'libx264', '-preset', 'fast', '-crf', str(a.crf), '-pix_fmt', 'yuv420p',
         '-c:a', 'aac', '-b:a', '160k', '-frames:v', str(total), '-t', str(total / 60), '-movflags', '+faststart', final])
    receipt['output'] = final.name
    receipt['probe'] = verify(final, total)
    (a.out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(final, flush=True)


if __name__ == '__main__':
    main()
