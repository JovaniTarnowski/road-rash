# Graphics options fixes — 2026-10-26

Request: fix the graphics-options title, help/popup overlap, inaccessible render
resolution and overlapping 2560x1920 window-size choice.

Baseline d9cc726. Private checkout `/Users/archit/Dev/rr/native-port-graphics-fix`
(branch `fix/graphics-options-20261026`) avoids the active shared-tree frontend
claims. Owned files: src/frontend.cpp, src/native_screens.cpp,
src/native_strings.cpp, tests/frontend_test.cpp, this handoff. No simulation or
original popup tables changed; original options remain the fidelity baseline.

## Changes

- Display link retains Graphics Options title and renders its full help in the
  native help region. Do not use the help sentence as a Badloc-72 title.
- Native help uses a wrapped 24px font in a dark panel above the popup, in both
  scale-1 and scaled frontend paths. Shortened resolution/style copy fits that
  panel. Empty values no longer produce a spurious Current line.
- Native popup rows use one template and a uniform 28px step, with separate
  header and disjoint 26px choice hit rectangles. Five choices fit inside 480px;
  Window Size now orders 640, 1280, 1920, 2560 correctly.
- Render Resolution stays focusable in Low/Inset. Its help explains that applying
  a value selects High race mode; applying does so and clears Inset. Race Low
  and Inset remain selectable through the original Race Resolution popup.
- Native setting changes refresh the Current help line immediately.

## Verification

Private build `build-graphics`, CMake Debug and exported compile commands.
`cmake --build build-graphics --parallel 6` passed. First build had overlapping
local invocations and linker failures; stopped the duplicate and reran serially
(one build invocation) successfully. Client C++ LSP unavailable; agent_lsp.py
with the private compilation database and clangd --check native_screens.cpp
passed with zero errors.

`frontend-test ../cdrom/ROADRASH --dump /tmp/rr-graphics-shots --menu-corpus
/Users/archit/Dev/rr/native-port/analysis/oracle` passed. Local missing frontend
original captures are reported as skips (19 lines), not parity evidence.
Focused ctest frontend/native_screens passed 2/2 after final source edits.
New regression checks cover every Window Size via pointer clicks, all native
popup row bounds/nonoverlap, Display title flags, and render choice application
from both Low and Inset. Style popup rendered at scale 1 and 2; scale-2 style,
scale-1 size and Display-hover screenshots visually inspected.

Evidence under shared `analysis/graphics-options-20261026/`: PNGs
rr-graphics-style.png, rr-graphics-window.png, rr-graphics-hover.png, focused and
frontend/build logs. Commercial assets/screenshots remain local and untracked.

Browser Release build via tools/build_web.sh passed and validated nine
code/metadata files. Bundle: private checkout's build-web-graphics/dist. First
configure lacked private .tools/emsdk; fixed with a local symlink to the shared
SDK, without modifying build configuration. No hosted deployment or browser
runtime smoke claimed.

Full native CTest completed including race-soak: 92 passed, 13 skipped and one
oracle-sim failure caused solely by the missing private analysis/oracle symlink.
After linking the existing local oracle fixtures read-only, rerunning the failed
and previously skipped checks is in progress (13/14 pass so far).

A second frontend run with all current local image captures found 18 original
screen image-floor failures. A separately compiled pristine d9cc726 checkout
(`/Users/archit/Dev/rr/native-port-graphics-baseline`) reproduces exactly the
same 18 failures. The active shared tree has an uncommitted scalar JPEG decoder
migration; its existing build-agent-menu-gaps/frontend-test passes these captures.
These baseline image comparisons do not originate in the menu changes. Logs:
/tmp/rr-graphics-pristine-full.log and /tmp/rr-graphics-frontend.log. Integration
will also run focused tests against the shared decoder migration.
