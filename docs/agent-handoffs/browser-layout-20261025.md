# Browser layout revision — October 25, 2026

Baseline `ae5ea41`. User requested removal of slogans/oversized heading,
much larger gameplay, then styling closer to the original game.

Owned: `web/index.html`, `web/style.css`, web status strings in `web/app.js`,
and this handoff. Narrow entries added to PROGRESS/VERIFICATION. OMP's dirty
`src/race.cpp` and ongoing simulation work are untouched/excluded.

Removed the marketing layout and decorative disc. Orange condensed headings,
yellow markers, gray bevel controls and dark panels echo the game's menu.
48 px toolbar and compact legal footer leave the rest of the viewport for the
uncropped 4:3 game. Controls are an on-demand overlay. No fixed canvas-size cap.

Validation: local Chromium/Playwright import + actual quick race. At 1440×900
canvas 1104×828; 1907×1280 canvas 1610.66×1208; 1280×800 canvas 970.66×728;
portrait 390×844 and landscape 844×390 fit correctly. All preserve 4:3, no
scrolling; controls popup and JS error checks pass. Screenshots/log in ignored
`analysis/browser-layout-20261025/`. No native regression gate needed for this
HTML/CSS/status-only edit; reused validated clean engine `096bff2` instead of
rebuilding OMP's in-flight simulation probes. No commercial assets in bundle.

Prepared/deployed via `prepare_web_deploy.py build-agent-browser-layout/dist`
and Vercel prebuilt preview on the existing project. Final URL recorded after
READY. Temporary local server port 8770 is owned by this session.


Release: shell `7d8f7d7` pushed to main, reused engine `096bff2`.
Preview READY: https://road-rash-32f5uhubr-arkits.vercel.app
Deployment `dpl_4GgoKCBX6fL6kUzQ8wr3ME7RAdiV`. Existing preview Vercel sign-in
applies. Deploy receipt copied to `analysis/browser-layout-20261025/`.
Local server stopped; no held resources. OMP's dirty race.cpp remains untouched.
