# Handoff: EXE-reader fuzz extension + libFuzzer driver false positive — October 15, 2026

## Extension (committed)

`fuzz/fuzz_exe_tables.cpp` now also drives `read_career_tables`,
`read_volume_curve`, `read_rider_remaps(bytes, 9)`, `read_road_materials(bytes,
bytes)` and `read_control_tables` (all bounds-checked EXE-image readers,
previously unfuzzed). No CMake/seed changes: same `seed-1.exe` + disc
`ROADRASH.EXE` corpus. 6969 runs, exit 0, no crashes.

## Driver false positive (environmental, NOT a project bug)

A bare run aborts before fuzzing a single byte:
`container-overflow ... __asan_memcpy` inside
`fuzzer::ListFilesInDirRecursive → ReadCorpora → FuzzerDriver → main`
(exit 134). Zero project frames; no crash artifact written. It is the
known libc++ container-annotation interaction in libFuzzer's own corpus
loader (file-count dependent: the music target with a smaller corpus
passed the same day on the pinned Homebrew clang 22.1.4).

Workaround used for the clean run (NOT committed):
`ASAN_OPTIONS=detect_container_overflow=0`. Do NOT put this in
`tools/fuzz.sh`: it would blind every target to genuine
container-overflows in project code. If the false positive blocks CI
fuzzing, the plan-22 owner should pick the harness policy (e.g. rebuild
the static fuzzer lib against matching headers, or split corpus dirs).
Evidence: `/tmp/goal-fuzzexe.log` (abort) vs `/tmp/goal-fuzzexe2.log`
(6969 runs, exit 0).
