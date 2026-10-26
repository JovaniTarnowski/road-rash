# Browser original fonts — October 25, 2026

Baseline e68a349; OMP concurrently committed fd2f07e (Android docs). User asked
for the page font to match the original game. Owned files: web/app.js and
web/style.css, this handoff, narrow PROGRESS/VERIFICATION entries. Shared
simulation work was preserved and excluded.

Native TextRenderer proves TEXT/BADLOC.DLL and FUTR/FUTB/FUTD.DLL are original
TrueType fonts. Browser FontFace now loads these same verified bytes from OPFS
after import and restoration, before launch is offered. Badloc headings,
Futura Bold buttons, Futura Regular body, Futura Demi key labels. No font
assets bundled or fetched; fallbacks before import and on unsupported loaders.
Browser antialiasing differs from thresholded native text; no pixel parity claim.

Validation: local Playwright/Chromium real-disc import, four loaded FontFaces,
computed font families, reload restoration, quick race, controls, portrait and
landscape mobile geometry, no font requests or JS errors. Repeated with Vercel
CSP. Evidence analysis/browser-fonts-20261025 (ignored). Node syntax and bundle
allowlist/reference-hash checks pass. Reused original validated wasm engine
096bff2; no native code changes or repeated native suite needed.

Release bundle build-agent-browser-fonts/dist; existing Vercel project preview
via prebuilt deployment. URL/status recorded after deployment. Temporary local
HTTP ports 8771/8772 owned by this session; stop at completion.
