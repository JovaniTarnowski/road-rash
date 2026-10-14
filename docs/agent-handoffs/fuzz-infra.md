# fuzz-infra

Task: Fuzz infra

Baseline: 97a42817e1bcf280cab1d57f147e45556a7f6bff

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-fuzz-infra`
Owner PID: 98772
Resource token: `d45c4c3d-58ed-42ed-917f-048a44f9ff06`

Owned files:

- `/Users/archit/Dev/rr/native-port/cmake/llvm.cmake`
- `/Users/archit/Dev/rr/native-port/fuzz`
- `/Users/archit/Dev/rr/native-port/tools/fuzz.sh`

## Progress / evidence

Plan 22 Task 2 (Steps 1-3) + Task 3 Step 1 implemented; verification running.

- `cmake/llvm.cmake` (new): fails with `brew install llvm` message when
  `/opt/homebrew/opt/llvm/bin/clang++` is missing. Homebrew clang 22.1.4 present.
- `CMakeLists.txt` (shared-file additive edit, needs integrator eyes):
  `option(RR_FUZZ ...)` + `fuzzer-no-link,address,undefined` compile flags
  (before `rr-resource` so the library gets coverage) and one executable per
  `fuzz/fuzz_*.cpp` + `fuzz-seeds` after it. FATALs on EMSCRIPTEN/ANDROID.
  Uses `-fsanitize=fuzzer-no-link` at compile so test/game mains still link;
  only `fuzz_*` executables link `-fsanitize=fuzzer`.
- `.gitignore`: added `analysis/fuzz/` (was NOT covered; only oracle/*.bmp etc).
- `fuzz/fuzz_common.hpp`: exact `rr_fuzz_contract` from the plan.
- `fuzz/seeds.cpp`: self-validating writer (re-parses every seed, exits 1 on
  mismatch); builders mirror tests/resource, wav, iso_fixture, cel, family,
  terrain, animation, avi, pe_strings, career tests. RRI seed is a locally
  generated 1x1 grey JPEG (331 B, Pillow); no original data anywhere.
- All 17 Task 3 targets drafted, one `rr_fuzz_contract` call each.
- `tools/fuzz.sh`: executable; builds `build-fuzz` with Homebrew clang,
  regenerates seeds (deterministic), runs each target with
  `-max_total_time=N -timeout=2 -rss_limit_mb=512`, crashes to
  `analysis/fuzz/crashes/<target>/`, exit 1 on any crash. `--seed-from-disc`
  maps disc extensions to targets (no source for iso/save/settings/unpack).

Deviations from the plan text (deliberate, recorded):
- `fuzz_iso` uses `list("")` for the volume root: `locate("/")` throws
  `Invalid ISO path` on ALL inputs (src/iso9660.cpp:85-92 skips nothing for
  the leading slash), so a literal `list("/")` target would never exercise
  list/read. One-level recursion, capped at 64 entries.
- `fuzz_avi` skips chunks pointing outside the input: `decode` takes a raw
  pointer and cannot report the overrun; ASan still guards the copy path.
- `fuzz_settings` has no contract wrapper: `parse_settings` must never throw,
  so any throw becomes a fuzzer crash by design.
- `fuzz_unpack` caps `expected` at 1 MiB; `fuzz_bobs` uses frame count 0x146
  (HIBOB count from tests/bob_test.cpp).

## Checks / remaining work

- [x] `tools/fuzz.sh 10 fuzz_settings` green (Task 2 Step 4):
      `Done 159133 runs in 11 second(s)`, avg 14466 exec/s, 2198 new units,
      slowest unit 0s, peak RSS 263 MB, exit 0, "all targets clean".
      Seeds writer self-checks all green (settings/cel/family/terrain/
      animation/bobs/avi/wav+ima/iso/pe-table+resources/rri/save roundtrips).
- [x] All 17 Task 3 targets compile under Homebrew clang 22.1.4 with
      `-fsanitize=fuzzer-no-link,address,undefined` (build-fuzz holds all
      `fuzz_*` binaries + `fuzz-seeds`); all 17 `fuzz/seeds/<target>/` dirs
      written (valid seed + `empty` each).
- [x] Full 10 s smoke across all 17 targets (final runner config: local-first
      corpus order, `-entropic=0 -reload=0`): 15/17 clean —
      animation 423k, audio 580k, avi 193k, bobs 128k, cars 325k, cel 329k,
      course 451k, exe_tables 236k, family 53, iso 23k, pe 316k,
      resources 583k, save 500k, settings 153k, terrain 593k execs each.
      rri + unpack found genuine OOMs (see findings). Seed-manifest guard
      passed (37 files, no pollution of the committed tree).
- [ ] Native gate: default Apple-clang build in `build-agent-fuzz-infra`
      (running in background as sh_13c3be459002VaoL8IQQFOpNHt) + a few quick
      parser CTests, then commit iff green.

## Toolchain finding (infra, Homebrew LLVM 22.1.4)

`libclang_rt.fuzzer_osx.a` aborts inside its OWN code under ASan, with zero
`rr::` frames. Two manifestations, both input-independent:
1. Entropic schedule + 2+ corpus files → heap-buffer-overflow in
   `InputCorpus::AddRareFeature` during seed load. Reproduces with two empty
   files. Worked around with `-entropic=0`.
2. `Fuzzer::RereadOutputCorpus` → `ReadDirToVectorOfUnits` →
   `ListFilesInDirRecursive`: container-overflow in `vector<string>` relocate
   once the listed dir holds several files. Caused by (my) runner bug v1,
   which passed seeds-first so libFuzzer wrote thousands of generated units
   into `fuzz/seeds/` (settings reached 1922 files). Fixed by local-first
   corpus order + `-reload=0` + a seed-manifest guard that fails the run on
   pollution. Bogus `crash-da39a3ee*` (empty-input) artifacts deleted.
Re-evaluate both flags when the LLVM bottle changes; never silence with
`detect_container_overflow=0` (would blind real parser OOBs).

## Parser findings (filed for owning sessions, Task 3 Step 3; NOT fixed here)

Reproducers under git-ignored `analysis/fuzz/crashes/` (disc-seeded runs
still open). Repro: `build-fuzz/<target> <file> -runs=1 -entropic=0 -reload=0`.
- F1 `unpack_resource` unbounded `resize(unpacked)` → OOM
  `malloc(4294967295)`, deterministic on 20 B input (`fuzz_unpack/oom-8dd9*`).
  src/explode.cpp must cap `unpacked` before allocating (Review Focus 3).
- F2 `explode` pathological slowness: 20 B input (`fuzz_unpack/oom-7a8e*`)
  takes ~91 s; real run also recorded `timeout-4386*` under `-timeout=2`.
  src/explode.cpp.
- F3 family 77 B input (`fuzz_family/oom-5beb*`) takes ~44 s (near-hang, no
  OOM on repro). Target calls both `read_family` and `read_cel_group`;
  minimizer must attribute to one. src/family.cpp and/or src/explode.cpp.
- F4 `decode_rri` unbounded stb_image allocation on fuzzed SOF dims → OOM
  (~0.6–2.7 GB; exact figure varies with rss pressure), deterministic on
  231 B input (`fuzz_rri/oom-28bd*`). src/rri.cpp should pre-check dimensions
  (e.g. `stbi_info_from_memory` + cap) before the pixel alloc. Legit RRIs are
  small, so a cap changes no valid behaviour.
Each fix belongs in its own commit with a hand-made-bytes regression test
per Task 3 Step 3; fixing touches src/* outside this session's claim.
- [ ] Native gate for touched code: `build-agent-fuzz-infra` Debug configure +
      build + `ctest -L unit`? Full gate touches other workers' files; at
      minimum build the default tree to prove the CMakeLists edit is benign.
- [ ] Task 3 Steps 2-4 (5-min synthetic + disc-seeded runs, fixes) open.
- [ ] Commit (only on green): `fuzz: libFuzzer build, synthetic seeds and runner`
      + `fuzz: targets for every parser of user data`, staging explicit paths
      only (CMakeLists.txt/.gitignore shared with other workers — integrator).
- [ ] TOOLING.md/VERIFICATION.md updates belong to Task 7; leaving to integrator.

Untouched as required: oracle/*, src/race.cpp, src/bike.cpp, src/traffic.cpp.
Note: tree contains other workers' in-flight edits (oracle/*, tests/settings_test.cpp,
tests/dash_bench.cpp); none touched.

## Live processes / resources

File claims above. Record capture/Ghidra processes here.
