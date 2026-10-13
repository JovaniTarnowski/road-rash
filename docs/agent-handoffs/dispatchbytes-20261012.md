# dispatchbytes-20261012

Task: Verify the sibling AI merge's dispatch table (`36c03dc`) by direct EXE read — no REA needed for data bytes. Read-only review; no edits to merged code.

Baseline: `8f48801` (own prior commit; `36c03dc` sits below it).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (incremental rebuild only; no owned build dir created)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet added via gap-PUT, AI-brain section)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/dispatchbytes-20261012.md` (this file)

No new `analysis/oracle/` files: the evidence is 8 dwords read directly from the hash-verified EXE at file offset `0x63f80` (the documented `.data` location of `0x00465980`):

`408e30 409000 4092d0 409460 409ca0 40a4f0 40a560 40a5b0`

## Progress / evidence

```python
struct.unpack('<8I', exe[0x63f80:0x63f80+32])
# → all 7 state targets exact, MATCH: True
```

(Todo named slot tables + countdown REA; data tables aren't REA-traceable and the countdown claim is behavioral — verified what was verifiable statically, recorded the rest as merge-test-owned. Substitution noted.)
The merge's `Race::countdown_step` (`dt 2 at tick 1`) and `collide_riders` (`0x004368f0` walk + `0x004373f0` gate + `0x00436a40` line) were read in full; their cited functions are all previously confirmed at export level. Behavioral parity stays with the merge's own replay test.

Shared-doc edit: one sub-bullet under AI-brain-finding via gap-PUT. No source, test, plan, or other doc file touched — merged files left exactly as committed.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- NOT done: behavioral re-verification of the merge; proxy capture fix; no captures run.
- Integrator: link from plan 10 Task 5/6 next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks.
Merged files reviewed read-only, never edited. Sibling uncommitted changes untouched. `oracle/`, prefix/install untouched.
