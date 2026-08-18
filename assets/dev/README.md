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
- **`bunny_pack.nvpack`** — the compiled runtime pack `src/app`
  actually loads at startup (`content::LoadPetPackFromFile`), built from
  `bunny_pack/manifest.json` by `tools/compile_pet_pack.py`. Binary,
  not meant to be read directly — see `docs/ANIMATION_RUNTIME.md` §4
  for the exact on-disk format.

Regenerate everything derived from `bunny_source.png` with one command:

```bash
python3 tools/generate_bunny_dev_pack.py
```

This is deterministic — an unchanged `bunny_source.png` always produces
byte-identical frame PNGs, manifest, and compiled pack.

`src/app/SpikeApp` loads `bunny_pack.nvpack` as the one pet it shows and
derives every frame's click-through hit-test region from that frame's
own real alpha channel (see `PetDefinition::alphaHitThreshold`, default
128). If the pack can't be loaded (e.g. the process isn't launched from
the repo root, where this relative path resolves from), the app fails
loudly — logs a specific fatal error and exits non-zero — rather than
falling back to any hardcoded shape; see `docs/DECISION_LOG.md` DEC-023.

**Superseded, no longer present:** `bunny.rgba` (Block 01's single-frame
uncompressed runtime format, loaded by the now-retired
`graphics::DevSprite`) has been removed — nothing loads it anymore. Its
producer function (`prep_dev_sprite.write_raw_rgba`) still exists in
`tools/prep_dev_sprite.py` as a small, harmless historical utility, but
that module's PNG codec functions (`read_png_rgba`/`write_png_rgba`) are
what Block 02's pipeline actually reuses.
