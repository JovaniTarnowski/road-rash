# Handoff: Plan 22 Task 3 Step 3 — Parser Fuzz Fixes (F1, F2, F3, F4)

- **Task / Plan**: Plan 22 Task 3 Steps 3–4 (Fix libFuzzer parser findings F1–F4 from `docs/agent-handoffs/fuzz-infra.md` with hand-made byte regression tests)
- **Baseline commit**: `314c9a0`
- **Commits**:
  - `29fd605` `fix(explode): cap unpacked size and reject uncompressed size mismatch`
  - `c2ab4d9` `fix(rri): cap decoded JPEG dimensions before allocation`
- **Worktree**: `/Users/archit/Dev/rr/native-port`
- **Build directory**: `/Users/archit/Dev/rr/native-port/build-agent-gemini-fuzz-fixes`
- **Owned & changed files**:
  - `src/explode.cpp`
  - `src/rri.cpp`
  - `tests/resource_test.cpp`
  - `docs/agent-handoffs/gemini-fuzz-fixes.md`

## Findings resolved

1. **F1 (`fuzz_unpack/oom-8dd9*`), F2 (`fuzz_unpack/oom-7a8e*`, `timeout-4386*`), F3 (`fuzz_family/oom-5beb*`)**:
   - **Root cause**: All four reproducers had `compressed = 0` (`u32(12) == 0`) with huge `unpacked` values (`4294967295`, `1526726660`, `343932928`, `1023410560`) and `stored = 0` or `4`. `unpack_resource` checked `stored <= unpacked` without capping `unpacked` or requiring `stored == unpacked`, causing `out.resize(unpacked)` to allocate up to 4 GiB (OOM or >2s ASan zero-fill timeout). Additionally, `explode()` called `out.reserve(expected)` upfront before reading the bitstream.
   - **Fix (`29fd605`)**:
     - Adopted `ByteReader` in `unpack_resource`.
     - Capped `unpacked` / `expected` at `1u << 24` (16 MiB — 35× the largest unpacked resource on the disc, 450,768 B).
     - Required `stored == unpacked` when `!compressed` (matching `0x00415550` / `0x004157b0` and all 13 disc containers, where `uncomp_mismatch == 0`).
     - Capped upfront `out.reserve(std::min<size_t>(expected, 65536))` in `explode()`.
   - **Regression test**: Hand-made bytes in `tests/resource_test.cpp` (`stored != unpacked` uncompressed header, oversized `unpacked` header, and `explode` > 16 MiB).

2. **F4 (`fuzz_rri/oom-28bd*`)**:
   - **Root cause**: A 231-byte JPEG with SOF0 dimensions `10496 x 65280` caused `stbi_load_from_memory` to allocate ~2.05 GB before padding missing MCUs.
   - **Fix (`c2ab4d9`)**: Added `stbi_info_from_memory` pre-check in `decode_rri` (`src/rri.cpp`) capping dimensions to `1..4096` (all disc `.RRI` images are at most `640 x 480`).
   - **Regression test**: Hand-made 1×1 JPEG header patched to SOF0 height `4097` (`0x10, 0x01`) in `tests/resource_test.cpp`.

## Verification

- `ctest --test-dir build-agent-gemini-fuzz-fixes -R "^(resource-bounds|family|cel|animation|frontend)$" --output-on-failure`: 5/5 PASS.
- `python3 tools/agent_lsp.py --database build-agent-gemini-fuzz-fixes/compile_commands.json --check src/explode.cpp` and `--check src/rri.cpp`: healthy CDB, 0 AST/compile errors.
- Reproducers (`./build-fuzz/fuzz_unpack`, `./build-fuzz/fuzz_family`, `./build-fuzz/fuzz_rri` with `-runs=1 -entropic=0 -reload=0`): all 5 crash/OOM/timeout inputs execute in 0 ms and exit 0.
- `sh tools/fuzz.sh 3 fuzz_unpack fuzz_family fuzz_rri`: all clean (`fuzz_unpack` 37,671 exec/s, `fuzz_family` 8,047 exec/s, `fuzz_rri` 350 exec/s, `slowest_unit_time_sec: 0`).

## Live processes / held resources

None; claim `gemini-fuzz-fixes` released after committing this handoff.
