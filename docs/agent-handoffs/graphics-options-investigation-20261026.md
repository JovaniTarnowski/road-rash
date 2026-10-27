# Graphics options investigation — 2026-10-26

Scope: investigate the five supplied graphics-menu screenshots. Read-only source
analysis at bfb23de with existing shared-tree modifications preserved. No runtime
capture or new original-binary measurement performed.

## Findings

- Race Resolution and Biker Resolution are original options, not enhancements.
  Original resolution popup 0x00474240 uses setter 0x004457f0; biker popup
  0x00474320 uses setter 0x00445900. See REVERSE_ENGINEERING.md (frontend
  popups and HiResBikers). Race modes are Low/Inset/High; High is 640x480,
  Low is 320x240, Inset is a 320x240 inset. Biker detail selects separate
  Hi/Lo sprite assets, rather than upscaling a single sheet.
- Display and its render resolution, widescreen, visual style and texture-detail
  options are native additions (native_screens.cpp/native_strings.cpp).
- Broken Display hover title: add_native_screens assigns the graphics link
  `title_desc | description`, with d_display_mode as its description.
  Frontend::set_focus therefore puts the entire help sentence in the Badloc-72
  title. value_item already explicitly avoids this flag; the graphics link
  missed the same correction.
- Render Resolution is deliberately disabled in Frontend::prepare_screen when
  `!(settings_.high_resolution && !settings_.inset)`. high_resolution defaults
  false. Disabled rows are skipped by pointer hit testing and navigation, so
  their explanatory d_render_lowres cannot be discovered by focusing them.
  Workaround: Graphics Options -> Race Resolution -> High, then Display.
- Visual Style popup leaves the description and Current value painted behind
  its header/choices. build_popup reuses original popup geometry without
  reserving room for the much longer native help text. The screenshot shows
  the header covering the Current line. Both software and scaled frontend
  render paths require verification after a layout correction.
- Window Size 2560x1920 is implemented (target 4 -> window_scale 4), but
  build_popup places choices at `t.y + i*t.h` while cycling three DIFFERENT
  template rows. This is not a monotonic layout; the fourth choice wraps to
  the first template y before adding its offset. The screenshot shows it
  between 1280 and 1920, overlapping the preceding choice's rectangle.
  Reverse-order pointer hit testing can obscure earlier rows in overlaps.
  It is a geometry/hit-target defect, not an unsupported-resolution limit.
  Keyboard selection may work; not runtime-verified in this investigation.
- Existing frontend_test covers keyboard persistence of render_scale=4, not
  pointer reachability of Window Size 2560x1920, popup rectangle separation,
  or text/header overlap. That test does not disprove the screenshot defects.

## Follow-up implementation

Use a single popup origin and uniform row step; fit the full header/choices to
480px. Allocate a separate native help region above that popup, with wrapping
and a dark backdrop. Remove title_desc from the Display link. Make the locked
Render Resolution explanation accessible (or offer a deliberate switch to High).
Verify all native popup sizes with pointer and keyboard input, both rendering
paths, and Low/Inset/High states.

No implementation edits: frontend.cpp/hpp, text.cpp/hpp and frontend_test.cpp
are claimed by the frontend-acceptance-all-20261026 worker; latest handoff has
pending status. This report owns only this new file and holds no resources.
No build needed for this documentation-only investigation. No fixes claimed.
