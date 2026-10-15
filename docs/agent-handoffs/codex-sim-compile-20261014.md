# Handoff: isolated SIM diagnostic compile prerequisite (October 14, 2026)

Plan 10 Task 5 prerequisite. Baseline `314c9a0c5e3118428c27c17fcf98000cebd1d3d9` plus preexisting OMP oracle diagnostics, preserved.

## Ownership and changes

Only shared write: this uniquely named handoff. No changes to shared oracle sources/build script, no installation, no Wine/REA run, no commit. Private directory created uniquely with `tempfile.mkdtemp`; no shared prefix/build/project resources acquired. All subprocess builds finished; no held resources or live processes.

Private evidence directory: `/var/folders/bj/fw1cj85n1yz0_rwt9mb__6bh0000gn/T/rr-codex-sim-compile-20261014-fh1qe1_a`.
It contains copies of `aweman_proxy.c`, `aweman_proxy.def`, `sim_fields.h`, `build.sh`, the exact preexisting shared diff in `original-shared-oracle.diff`, before/after build logs, declaration-only patch, DLL and `receipt.json`. Original source copy is reconstructible from the shared-source SHA and preserved diff; private source contains the fix below.
Shared oracle C SHA-256 checked unchanged before handoff: `c26a88ccd50394fc4f33e487e98713c7afe3d06a04849e11c2e35a9cb515f952`.

## Reproduction and minimal repair

Built directly inside the private directory with the compiler/link flags from the copied build script (resource wrapper bypassed because directory is private):

```sh
i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -o AWEMAN32.DLL aweman_proxy.c aweman_proxy.def -lwinmm -Wl,--enable-stdcall-fixup
```

Unmodified copy: compiler exit **1**, `aweman_proxy.c:64:57: error: 'g_frame' undeclared`; also unused `g_frame_fwd` warning at line 54.

Minimal exact repair: replace `static uint32_t g_frame_fwd;` at line 54 with `static uint32_t g_frame;`, and remove its old declaration after `orig_frame` (line 559 in original copy). One variable, now declared before `rr_fault_log`; no behavior logic changed.

Repaired private copy: compiler exit **0**, empty stdout/stderr (no warnings). Compiler statuses retained independently in receipt; success of the orchestration script does not replace them.

## Remaining experiment / limitations

This proves compilation only, not SIM capture stability. Do not install this DLL as accepted instrumentation yet. The PAGE_GUARD diagnostic remains flawed: it traps any access, clears protection after the first self-module access, and accepts unrelated guard exceptions. Existing `hook_project` reads `g_frame` during `orig_frame`, so it can consume the one-shot guard before the suspected corruption. Prefer a write watchpoint on the regressing global, or a narrowly scoped guard handler with access-type/address checks and deliberate re-arming. Preserve the previous owner’s dirty source while coordinating any shared integration.

Latest OMP tail stopped immediately after the failed guard build and erroneous `g_frame_fwd` addition; provider limits prevented the correction/rerun. Existing menudbg log contains no guardtrip, shows `g_pixels=8347995` after corruption through tick85, and ends in projector entry at frame30. Timer-kill calls there returned no armed timers. This supersedes the earlier generic two-tick/timer-suspect handoff; corruption source is still unproven.
