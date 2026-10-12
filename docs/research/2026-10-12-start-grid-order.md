# Original start-grid draw order — October 12, 2026

The original's ordering is recovered from the disc executable and supported by
two saved original frame-0 display-list tails. The current front-half actor delay
in `src/course_scene.cpp` fixes the reported tyre clipping but does **not** implement
the recovered ordering in full. This is research evidence, not native frame parity.

## Reference and REA

Reference: `../../../cdrom/ROADRASH/ROADRASH.EXE`, SHA-256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
The hash agrees with `analysis/inventory.json`; the patched Android EXE was not used.
Commands ran from `native-port/`, using the environment in [TOOLING.md](../TOOLING.md):

```sh
export PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"
export GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00443af0 --format json
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00438610 --format json
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00443cd0 --format json
```

All three calls exited 0, used Ghidra 12.1.4, reported the reference digest,
and returned pseudocode byte-for-byte identical to the existing address-keyed
exports. Complete assembly was checked for calls, stride, comparison widths
and callback arguments. Local, git-ignored evidence:

- `analysis/oracle/rea-start-grid-scene-order.json`: `0x00443af0..0x00443b31`.
- `analysis/oracle/rea-start-grid-object-order.json`: `0x00438610..0x00438790`.
- `analysis/oracle/rea-start-grid-world-order.json`: `0x00443cd0..0x00443d68`.
- `analysis/oracle/start-grid-order-audit.json`: export comparisons, capture
  integrity checks, scene hashes and decoded node coordinates.

REA imports ephemerally; the annotated Ghidra project was not modified. This
is an independent import through the same decompiler engine, not a second runtime.

## Scene loop: the whole nearer world strip precedes farther objects

`0x00443af0` starts at projected record `0x004c0a5c`, index 31 of
`0x004bc800` with a **0x224-byte** stride (`int * -0x89` in pseudocode).
Define `W(j)` as `0x00443cd0` on record j and `O(j)` as `0x00443b40` on j:

```text
initialize object cursor
W(31)
for j = 31 down to 1:
    W(j-1)
    O(j)
    commit object cursor
O(0)
```

The decisive assembly is `0x00443b08` (subtract 0x224), `0x00443b10`
(nearer world), `0x00443b15` (farther record as object argument),
`0x00443b1b` (objects), `0x00443b20` (cursor commit). No fraction test occurs here.

On a single road, `0x00443cd0` emits lanes through `0x0043c170` and
**both terrain sides** through `0x0043bef0` before returning. The latter
dispatches shoulders/hills/buildings/marked terrain by sub-slice type and flags.
On a split, it emits both roads' lanes, both terrain sides, then join helper
`0x0043be80`, before any `O(j)` call. This establishes helper ordering;
native split stitching remains open.

At camera sample 49, an actor at sample 50 is processed by `O(1)` after
the complete `W(0)` strip between samples 49 and 50. Its tyres can extend
below the sample-50 edge without that road strip overwriting them.

## The 0x80 split inserts scenery between actors

`0x00438450` builds visible array `0x004c3d30` from the scheduler ring,
preserving ring order, and copies actor position `+0x1c` to rendered position
`+0x44`. `0x004385f0` initializes the cursor to count minus one.
`0x00438610` walks backwards, stopping when **signed** rendered position
`>>8` is below the projected sample. Actor `+0x54` must match the projected
road's `+0x0c` sub-slice pointer. Lateral bounds include the lower edge and
exclude the upper edge, after truncating signed `+0x50` Q8 toward zero.

Inside this object pass, when projected depth `+0x18` is greater than **0x92**:

1. Emit roadside records selected by mask `0x00000ff8`, if present.
2. Visit matching actors in reverse visible-array order.
3. Before the first matching actor whose rendered-position low byte is
   **less than 0x80**, emit remaining roadside records with mask
   `0xfffff007` and placed scenery through `0x0044b5f0`.
4. Emit that actor and the remaining front-half actors.

If no matching front-half actor exists, step 3 occurs after the scan instead.
The threshold is an **unsigned byte** comparison (`cmp al,0x80; jae` at
`0x004386a7..0x004386a9`): 127 triggers it, 128 does not. Insertion occurs
before lateral-bound and non-null callback checks; an actor that contributes
no pixels can still trigger it. Depths <=146 suppress scenery, not actor callbacks.

Actor callback `+0xdc` receives ECX = actor, EDX = projected road
(`0x00438714..0x0043871a`). Bike dispatch `0x00446250` emits shadow then
body; armed dispatch `0x004463e0` adds a weapon overlay afterward. Equal-position
actors retain the reverse of the actual visible-array order; native stable sorting
over a different rider/car enumeration does not prove equivalent ties.

## Saved original start-frame evidence

Both captures have the reference digest and an `end` marker. `napa-start.tsv`
has frame 0; `napa-drive.tsv` has contiguous frames 0–900. Their v3 scene-0
dumps parse exactly to the recorded node count and file length. Both use
camera position 12544 (sample 49), x 241, height 150, high-resolution mode.

| Capture, frame 0 | Near road nodes | Following world node | Near shadow | Near body |
| --- | --- | --- | --- | --- |
| `napa-start` | 9–11 | 12 | 13 | 14 |
| `napa-drive` | 4–6 | 7 | 8 | 9 |

Indices are zero-based. The three road quads run from logical y **152.5**
to **465**. Shadow/body use dynamic Bob resource `0x00466684`, frame **47**,
colours **0xe0**/**1**. Body spans x **185.5–214.5**, y **97.5–164.5**;
shadow spans x **187.5–214.5**, y **149.5–161.5**. Thus the actor extends
below the 152.5 seam, but follows the nearer road and subsequent world geometry.

These are pending-list **tails**, not every node emitted for the frame:
`0x00450470` can flush full lists during construction. There are 15 pending
nodes in `napa-start` and 10 in `napa-drive`. They support near-grid layering
and shadow/body order, not every actor tie or complete native scene construction.

## Native difference and correction sequence

The current `CourseScene::draw_nodes` workaround queues only actors with
fraction <128, inserts them after the next nearer road panels/marked ground,
and does so **before** nearer shoulders/hills/buildings. Back-half actors and
scenery still emit alongside their own world strip. The original puts the
**whole** nearer world pass before the farther object pass for both actor halves,
and applies the fraction split only inside the object pass.

Separate world-strip emission from per-slice object emission, following
`W(31), W(30), O(31), W(29), O(30), …, W(0), O(1), O(0)`. Keep the farther
slice's actor projection/occlusion state in `O(j)`; using the nearer slice changes
scale and position. Preserve the scenery mask groups and scheduler-derived tie
order rather than assuming one scenery batch or native rider enumeration matches.

No native source was changed in this research task. Plan 11a Task 3 acceptance
remains open: v3 dumps lack scheduler actor objects needed to rebuild all actors.
Extend the capture with full bike/rider state and compare native constructed
nodes, including fractions 0/127/128/255, equal-depth rider/car cases and a hill
or building, before claiming complete frame parity.
