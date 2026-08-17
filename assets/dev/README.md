# assets/dev

Reserved for development-only placeholder assets (never final art —
see `docs/PET_CONTENT_SPEC.md` and `AGENTS.md` §11).

Block 01's spike doesn't put anything here: its placeholder creature
(`core::BlobSilhouette`, see [`src/core/Silhouette.h`](../../src/core/Silhouette.h))
is generated procedurally from plain math, not loaded from a file, so
there is currently no bitmap/asset to check in. This directory exists
now so future blocks that *do* need a loadable dev placeholder (e.g. to
exercise a real content-loading path) have an obvious, pre-agreed place
for it that's clearly separated from any future real asset pipeline.
