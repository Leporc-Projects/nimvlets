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
  validate hit-testing against real alpha data, first directly (Block
  01) and now as the source image Block 02's derived DEV pack below is
  generated from.
- **`bunny_pack/`** — the Bunny **DEV animation pack**'s source
  material (Block 02, see `docs/ANIMATION_RUNTIME.md` §5): `frames/*.png`
  (7 deterministically-derived PNG frames — idle, click squash/stretch/
  settle, passive lean-left/lean-right/settle) plus `manifest.json`
  (the `tools/compile_pet_pack.py` input describing how they combine
  into idle/click-reaction/passive animations). Regenerated in full by
  `tools/generate_bunny_dev_pack.py`; never hand-edited.
- **`bunny_pack.nvpack`** — the compiled runtime pack, built from
  `bunny_pack/manifest.json` by `tools/compile_pet_pack.py`. Binary,
  not meant to be read directly — see `docs/ANIMATION_RUNTIME.md` §4
  for the exact on-disk format.
- **`pet_catalog_manifest.json`** — the Block 04 catalog manifest (see
  `docs/CATALOG.md`): a single entry, Bunny, marked default. Hand-
  written, not generated — there's no derivation step for a catalog
  the way there is for pixel frames.
- **`pet_catalog.nvcat`** — the compiled catalog `src/app` actually
  loads at startup (`catalog::LoadCatalogFromFile`), built from
  `pet_catalog_manifest.json` by `tools/compile_pet_catalog.py`.
  Binary, not meant to be read directly — see `docs/CATALOG.md` §3 for
  the exact on-disk format. Regenerate after editing the manifest:
  `python3 tools/compile_pet_catalog.py assets/dev/pet_catalog_manifest.json assets/dev/pet_catalog.nvcat`.

Regenerate everything derived from `bunny_source.png` with one command:

```bash
python3 tools/generate_bunny_dev_pack.py
```

This is deterministic — an unchanged `bunny_source.png` always produces
byte-identical frame PNGs, manifest, and compiled pack.

`src/app/SpikeApp` loads `pet_catalog.nvcat` at startup and resolves
which pack to load against it (see `docs/CATALOG.md`) — with only one
real entry, that's always `bunny_pack.nvpack` today. It derives every
frame's click-through hit-test region from that frame's own real alpha
channel (see `PetDefinition::alphaHitThreshold`, default 128). If
either the catalog or the resolved pack can't be loaded (e.g. the
process isn't launched from the repo root, where these relative paths
resolve from), the app fails loudly — logs a specific fatal error and
exits non-zero — rather than falling back to any hardcoded shape; see
`docs/DECISION_LOG.md` DEC-023 and DEC-031.

**Superseded, no longer present:** `bunny.rgba` (Block 01's single-frame
uncompressed runtime format, loaded by the now-retired
`graphics::DevSprite`) has been removed — nothing loads it anymore. Its
producer function (`prep_dev_sprite.write_raw_rgba`) still exists in
`tools/prep_dev_sprite.py` as a small, harmless historical utility, but
that module's PNG codec functions (`read_png_rgba`/`write_png_rgba`) are
what Block 02's pipeline actually reuses.
