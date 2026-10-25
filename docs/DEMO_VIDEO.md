# Offline gameplay demo videos

`tools/render_demo.py` renders the actual native race scene, sprites and HUD,
using the existing native autopilot. It advances exactly one 60 Hz simulation
tick per exported frame and pulls the native stereo mixer at 22,050 Hz. This
is engine output, not a screen recording or an original-game capture.

From the repository (macOS or Linux with SDL3, CMake, Python 3, FFmpeg with
libx264, and ffprobe):

```sh
python3 tools/render_demo.py
```

The default Release build directory is `build-demo-video`.

The default is a **120-second, 1280×960, 60 fps MP4**, H.264/yuv420p video,
AAC stereo audio and fast-start metadata. It joins five 24-second clips in
quick-race order: Napa (0), City (1), Peninsula/HIWAY (2), Sierra/CANYN (3),
Pacific Highway/MEDLY (4). Six seconds of simulation run before each clip,
skipping the countdown and initial acceleration. Original 4:3 framing keeps
the full dashboard. Render scale 2 draws the scene at 1280×960. The original
visual settings are used; enhancements and external upscaled artwork are off.

Outputs default to a fresh `analysis/demo-video/YYYYMMDD-HHMMSS/` directory:

- `road-rash-native-demo.mp4`: assembled upload copy.
- `01-course-0.mp4` through `05-course-4.mp4`: individual clips.
- `receipt.json`: embedded build identity, frame counts, course parameters,
  engine commands and verified output streams.
- Engine/encoder logs per course, plus a build log when building.

The script refuses to overwrite an existing output directory. All videos,
local original assets and rendered frames stay out of Git. The capture has
native engines and effects; race soundtrack playback is deliberately disabled.
No external music or title cards are added. It never posts to social media.

## Variants

```sh
# Two minutes of continuous Napa gameplay; select a fresh output directory.
python3 tools/render_demo.py --courses 0 --seconds 120 --out analysis/demo-video/napa-2min

# 30-second sampler, or change course order, level and bike.
python3 tools/render_demo.py --seconds 30 --courses 3 0 1 --level 2 --bike 0

# Reuse a specific built executable; omit --no-build to rebuild first.
python3 tools/render_demo.py --build build-agent-demo-video-20261024 --no-build

# Keep the start countdown, or start further into each race.
python3 tools/render_demo.py --warmup 0
python3 tools/render_demo.py --warmup 900
```

`--assets` selects the extracted original `ROADRASH` directory, defaulting to
`../cdrom/ROADRASH`. `--render-scale 1..4` controls internal scene resolution;
export framing stays 1280×960. `--crf` changes encoding quality (default 18).
The simulation still uses the native port's present gameplay implementation;
a demo video does not establish original-game fidelity. A long single-course
capture can include crashes, arrests or the finish; the driver is an autopilot,
not a curated human control recording.

## How the engine capture works

The CLI accepts `--capture-video PATH --capture-audio PATH --capture-frames N`
and optional `--capture-warmup TICKS`. These enable a hidden SDL window with
vsync off, a fixed launch seed, original presentation, and no physical audio
device. Keyboard/gamepad/focus events cannot pause or steer capture; quit
still cancels, and incomplete output returns failure. Startup fades are skipped.
Normal interactive gameplay keeps its existing timing, input and audio path.

Every frame goes through `RaceScreen` and `SDL_RenderReadPixels` before present.
RGB24 rows are exported without pitch padding at 1280×960. Mixer samples are
stereo signed 16-bit little-endian; alternating 367/368 sample frames preserve
22,050/60 exactly, including odd warm-up lengths. Warm-up frames advance both
simulation and audio, then discard output. Interpolation samples the current
tick. No render/capture operation draws from simulation randomness.

The Python script passes an anonymous pipe to FFmpeg, so there is no multi-GB
raw video on disk and encoder failure closes the pipe. Only temporary PCM
and encoded video are stored, then cleaned. Clips have exact frame-count and
sample-count checks; final assembly trims AAC padding and verifies duration,
codec, dimensions, pixel format and all 7,200 frames for the default run.
The wrapper uses POSIX descriptor inheritance (`/dev/fd`); Windows support is
not provided. Hidden SDL rendering still needs a working video backend; this
is offline rendering, not a GPU-free/headless renderer.

## Verification

```sh
python3 tests/demo_capture_test.py build-agent-demo-video-20261024/rr-game
python3 tools/render_demo.py --build build-agent-demo-video-20261024 --no-build --seconds 1 --warmup 0
ctest --test-dir build-agent-demo-video-20261024 --output-on-failure
```

The local-disc integration test checks exact RGB/PCM lengths, moving/non-flat
frames, non-silent audio, invalid CLI rejection, and matching pixel/audio hashes
across launches separated by a wall-clock seed boundary. It leaves no fixtures.
The all-course short render exercises pipe encoding, AAC muxing and final joins.
This verification is export coverage, not a new simulation-parity claim.
