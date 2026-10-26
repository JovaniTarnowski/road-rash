# Web asset hosting size review — October 25, 2026

Scope: read-only review of the browser asset path and Vercel size feasibility;
no runtime, deployment configuration or commercial asset changes.
Baseline inspected: fd2f07e. Working tree was clean. Latest browser ownership
handoff: browser-layout-20261025.md (completed/released; no held resources).

## Measured sizes

Measured logical file bytes using pathlib.stat(), not allocated disk blocks.
Matched all 341 paths in src/reference_hashes.inc to ../cdrom/ROADRASH.

| Content | Files | Bytes | Decimal MB |
| --- | ---: | ---: | ---: |
| Original ISO | 1 | 518402048 | 518.402 |
| Entire mounted disc files | — | 516569918 | 516.570 |
| ROADRASH folder / browser import set | 341 | 489500591 | 489.501 |
| VIDEO | 59 | 310021274 | 310.021 |
| AUDIO | 103 | 153178604 | 153.179 |
| DATA | 22 | 17043230 | 17.043 |
| IMAGES | 146 | 7802411 | 7.802 |
| TEXT | 9 | 875488 | 0.875 |
| Root files | 2 | 579584 | 0.580 |
| Current layout web bundle | 7 | 3653621 | 3.654 |

The imported set is 466.824 MiB. Combined layout bundle plus all imported
assets is 493154212 bytes (493.154 MB). Largest game file is VIDEO/DUEL.AVI,
54578372 bytes (54.578 MB). No compression estimate was made.

## Existing browser behavior

web/app.js imports a user-selected ISO or folder, verifies the reference set,
and stores files in OPFS. Subsequent visits restore that local import. The
picker reads local files; it does not upload the disc to Vercel. Game reads
resolve through engine.localAssets.read. Automatic hosted assets require a
network provisioning path, progress/error handling, verified cached files and
appropriate deployment packaging. tools/prepare_web_deploy.py currently permits
exactly seven code/metadata files and rejects other bundle contents. The CSP
connect-src permits only self, so direct Blob fetches need a policy update.

## Hosting feasibility

Official docs checked through public web on this review date. Vercel connector
returned UNAUTHORIZED / reauthentication required; no account plan was inferred.

- https://vercel.com/docs/limits#static-file-uploads: CLI source uploads total
  100 MB on Hobby, 1 GB on Pro. This is an upload total, not a per-file serving
  limit. Full assets plus engine fit Pro; they exceed Hobby for this upload path.
- https://vercel.com/docs/vercel-blob/usage-and-pricing: Blob available on all
  plans, maximum object 5 TB, cache limit 512 MB/object, multipart recommended
  above 100 MB. Hobby includes 1 GB storage and 10 GB Blob data transfer.
  The 489.5 MB set fits storage; 10 GB is only about 20 full downloads before
  accounting for other use. Transfer/requests remain relevant as traffic grows.

Recommendation: provision the verified extracted set rather than an ISO;
reuse OPFS and reference hashes. Pro can serve the set as deployment static
assets, or Blob can separate asset storage from the web deployment on either
plan. Individual files allow later on-demand media loading. Existing full-set
readiness currently requires every reference, so lazy provisioning is additional
work, not existing behavior.

Repository AGENTS.md currently requires commercial assets to remain local/out
of packages. This investigation does not change that policy or publish assets.
An implementation should explicitly reconcile that policy with the requested
hosted-data behavior before publication.

Validation: file inventory/stat measurement and code inspection only; no builds
or gameplay tests needed for this research note. No live processes or resources.
