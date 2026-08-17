# assets/dev

Reserved for development-only placeholder assets (never final art —
see `docs/PET_CONTENT_SPEC.md` and `AGENTS.md` §11).

## Contents

- **`bunny_source.png`** — a real, non-analytic illustrated asset
  ("Bunny"), supplied by the repository owner as a **temporary QA
  fixture** for Block 01's macOS closure testing (see
  `docs/DECISION_LOG.md` DEC-018 and `docs/PLATFORM_SPIKE.md` §6).
  Resized to 320×320 (`sips`, alpha preserved) from the original
  1254×1254 source. **Not** the start of the real content-loading
  system `docs/PET_CONTENT_SPEC.md` describes, not a Nimvlet, and not
  referenced by product docs (`docs/PRD_V1.md`) — used solely to
  validate hit-testing against real alpha data before this block's
  interactive QA closed.
- **`bunny.rgba`** — `bunny_source.png` converted by
  [`tools/prep_dev_sprite.py`](../../tools/prep_dev_sprite.py) into a
  trivial uncompressed RGBA8 format (see that tool's docstring for the
  exact layout), so `graphics::DevSprite` can load it at runtime with
  no PNG decoder and no `SDL_image` dependency. Re-run the tool by hand
  if `bunny_source.png` ever changes:
  ```bash
  python3 tools/prep_dev_sprite.py assets/dev/bunny_source.png assets/dev/bunny.rgba
  ```

`src/app/SpikeApp` loads `bunny.rgba` as its default visual and derives
its click-through hit-test region from Bunny's own alpha channel (see
`graphics::DevSprite::kHitTestAlphaThreshold`). If the file can't be
loaded (e.g. the process isn't launched from the repo root, where this
relative path resolves from), the spike falls back to the original
analytic placeholder shape
(`core::BlobSilhouette`, see [`src/core/Silhouette.h`](../../src/core/Silhouette.h))
unchanged — that shape is still what `tests/SilhouetteTest.cpp`
exercises directly, independent of which visual the running app
happens to show.
