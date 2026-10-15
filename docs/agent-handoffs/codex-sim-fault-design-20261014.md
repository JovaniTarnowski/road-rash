# SIM capture forensic review — October 14, 2026

Baseline reviewed: `c2ab4d9` shared tree, including preexisting OMP diagnostic modifications in `oracle/aweman_proxy.c` and `oracle/build.sh`. Read-only source investigation; no Wine processes launched and no oracle files edited. Main integrator owns runtime captures and fixes.

## Concrete corruption mechanism

`hook_billboard` (shared-tree lines 1435–1462 at investigation) has two defects already present in commit `480022a`:

- It never forwards the intercepted call to original `0x0044ba40`.
- It writes `g_bb[g_nbb]` and increments `g_nbb` with no capacity check.

The only counter resets are `flush_actor_records()` and `dump_frame()`. SIM mode skips `dump_frame()` in the frame wrapper, so `g_nbb` accumulates across presents and eventually exceeds the 512-element buffer. `hook_select` already forwards and caps its own count, so its adjacent buffer does not share this overflow.

Local built DLL symbols from `i686-w64-mingw32-nm -n oracle/build/AWEMAN32.DLL`:

| Symbol | Address |
| --- | --- |
| g_nbb | 0x62f2e060 |
| g_bb | 0x62f2e080 |
| g_sel | 0x62f380a0 |
| g_output | 0x62f518a0 |
| g_pixels | 0x62f519b4 |
| g_frame | 0x62f519b8 |
| g_sim_frames | 0x62f91b14 |

`sizeof g_bb[0] == 80`. `g_pixels - g_bb == 0x23934 == 1821*80+36`: the `occ_owner` field of out-of-bounds record 1821 writes `g_pixels`. `g_frame - g_bb == 0x23938 == 1821*80+40`: the adjacent `occ_row` field writes `g_frame`. The counter itself is below the buffer and thus survives this forward overflow. This establishes a source and binary-layout mechanism for precisely the observed pair of corrupted globals without needing an additional watchpoint.

## Minimal repair and preservation requirements

Forward `0x0044ba40` before recording/returning on capacity. Preserve pre-call emu inputs; read `e2` after forwarding. Use `if (!g_out || g_nbb >= 512) return;` before every record-array write. Reset both actor counters at each frame entry so the collection lifetime is one original present even when SIM bypasses `dump_frame`. Keep the cap even with reset: one frame is not a proof that fewer than 513 actor calls happen.

Original disassembly confirms ECX/EDX first arguments and five stack arguments: `ret 0x14` at `0x0044bd29` and `0x0044bd3c`. Therefore the existing declared `void (__fastcall*)(int*,unsigned*,int,int,int,unsigned,char)` is consistent with stack cleanup. Reference: `analysis/original-disassembly.txt` function beginning line 104264.

No broad rewrite of the in-flight OMP diagnostic file is required. Integrator plans an isolated clean-source fix and preserves those diagnostics separately.

## Regression design

Preferred meaningful synthetic test: extract actual selector/billboard wrappers and record declarations into `oracle/actor_record_hooks.h`, included at their original location. Provide injectable original function pointers or a test-only callback macro. Native C test compiles that exact production hook body with synthetic objects and stubs for memory reads/readability. Test more than 512 calls without a flush; assert original forwarding for every invocation, capped record count, unchanged sentinels, exact forwarded args, pre-call emu snapshot and post-call display-list count. Explicit frame-reset helper used by production wrapper must reset both counts; invoke it from tests and verify a new frame can collect again. Saturation must never suppress original rendering. Avoid source-string assertions or a test-only rewrite of the wrapper.

## Optional diagnostic instrumentation (superseded by concrete cause)

The temporary PAGE_GUARD code observes the first access, which may be a harmless self read, disables the guard, and never rearms. It is not a persistent write watch. It references `g_frame` before its declaration; `g_frame_fwd` is an unrelated variable.

If future unknown writes need instrumentation, a PAGE_READONLY write-access-violation trap is more direct: filter owned page and `ExceptionInformation[0] == 1`; log faulting EIP/address and registers before changing page permission. To continue past legitimate writes, temporarily restore RW, set ContextRecord EFlags TF (0x100), then on the matching thread's single-step restore RO and clear only the TF introduced by the diagnostic. Keep handler state on another allocated page; avoid stdio/reentrant accesses to the watched page while protected. Read-only protection still catches unrelated writes on the same page, so either place watched globals on a dedicated allocation or classify carefully. No claim of runtime verification under this Wine installation is made.

Hardware watchpoints require debug registers for the actual guest thread. A helper thread can suspend the target, get/set CONTEXT_DEBUG_REGISTERS and resume; setting context on running self is not a reliable design. For two aligned 4-byte write watches: Dr0/Dr1 target addresses, Dr7 local enable bits 0 and 2; RW/LEN nibble 0xd at shifts 16 and 20. Data-breakpoint EIP is after the triggering instruction; a page write fault EIP is at it. This path was not necessary or tested here.

## Checks / resources

Used rg/fd, Git source/history inspection, original x86 disassembly, DLL nm symbol evidence. REA `npx --yes rea-agents@6.4.0 function ... 0x44ba40 --format md` launched in an ephemeral independent Ghidra import; output under `analysis/codex-sim-fault-design-20261014/`. REA completed successfully; recovered signature is `void __fastcall FUN_0044ba40(int*, undefined4*, int, int, int, uint, char)`, independently confirming the forwarding signature. No Wine/prefix resources held.

## Implemented regression handoff (isolated tree)

At integrator request, owned files created in `/Users/archit/Dev/rr/.codex-sim-20261014/native-port`:

- `oracle/actor_record_hooks.h`: exact extracted selector/billboard record structs/counters/wrappers; injectable default original functions using existing fastcall signatures; repaired billboard forwarding and cap; `reset_actor_records` helper. Production default guest reads are direct. Read-access macros permit the native test to translate fake 32-bit guest addresses into host storage without fragile low-memory mappings. `emu` local uses uintptr_t for native pointer readability; the record stores the original uint32_t width.
- `oracle/actor_record_buffer_test.c`: invokes actual production header with synthetic actor/descriptor/context data. Checks 4099 original calls per wrapper, exact arguments, descriptor/pre-call emu and post-call display-list snapshots, both 512-record caps, unchanged buffers after saturation, reset, collection after reset, and forwarding with output disabled.

Checks: strict `cc -std=c11 -Wall -Wextra -Werror` compile/run passed; separate address+undefined sanitizer compile/run passed; `clangd --check=oracle/actor_record_buffer_test.c --log=error` exited 0 (fallback source check, no exposed client LSP). Mutation experiments on temporary copies removed billboard forwarding, cap, and reset separately; all three runs failed, proving regression checks detect those required behaviors. Temporary mutation copies self-cleaned. No original assets used.

Integrator owns replacing main-source block with include, calling reset at frame entry, CMake registration, full build and runtime validation. Header/test are ready and no worker changes remain in flight. No commit performed by this worker.
