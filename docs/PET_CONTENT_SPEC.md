# Nimvlets — Pet Content Spec (initial contract)

This describes the shape a Nimvlet's content will eventually take. Block
01 implemented none of it. Block 02 implements a real (if
development-only, non-final) loader/runtime for a meaningful subset —
see §"Block 02's relationship to this spec" below for exactly which
fields are and aren't covered. Nothing here is committed to being the
*final* production serialization — `content::PetPackLoader`'s "NVPACK1"
format (see `docs/ANIMATION_RUNTIME.md`) is explicitly dev tooling, not
a claim that the shipped content pipeline will look the same.

## Why this exists now

AGENTS.md §13 requires that per-creature behavior be expressible as
content/data rather than hardcoded per-species C++. This document is
the placeholder for that data contract so later blocks don't
accidentally hardcode a frog's hop or a wolf's idle animation into
engine code.

## Fields a Nimvlet's content record must eventually carry

- **stable pet id** — a permanent, never-reused identifier (not a
  display name; names can change, ids can't).
- **display name** — user-facing (e.g. "Rin Rin").
- **purchase price in clicks** — integer, spent permanently on unlock
  (see `docs/DECISION_LOG.md` DEC-003).
- **starter eligibility** — whether this Nimvlet can appear on the
  first-launch starter choice screen.
- **hidden/secret eligibility** — whether this Nimvlet is a
  secret/easter-egg starter (see DEC-011) rather than a normally
  offered one.
- **base visual size** — the creature's natural on-screen footprint.
- **anchor/pivot** — the point used for window placement and any future
  drag/physics math, independent of the sprite's raw bounding box.
- **thumbnail** — a small preview asset for Shop/Collection UI (does
  not exist yet).
- **one mandatory idle animation** — every Nimvlet must define at least
  this; it's what plays when nothing else is happening (see Block 01's
  spike blob, which stands in for this with a two-circle bob).
- **zero or more optional animations/actions** — species-specific
  behaviors (a hop, a croak, a tail wag) expressed as data, not code.
  Different Nimvlets are explicitly allowed to have different numbers
  of these — nothing requires uniformity.
- **atlas/frames** — however the animation's individual frames are
  packed; format not decided yet.
- **timing per frame** — duration of each frame, so frame rate isn't
  hardcoded per creature.
- **looping** — whether an animation loops or plays once.
- **alpha/hit shape** — the non-rectangular region used for click
  detection and click-through, analogous to Block 01's
  `core::BlobSilhouette::Contains()`, but eventually per-pixel/per-frame
  rather than two analytic circles.
- **provenance metadata** — where the asset came from (useful once a
  content pipeline like Ludo.ai is in play — not in Block 01's scope).
- **package/schema version** — so old content records can be migrated
  forward instead of silently breaking when the format changes.

## Explicit rules

- Not every Nimvlet needs the same number of animations/actions.
- "Personality" is not a required technical system — species-specific
  flavor should live in animation/data choices, not a mandatory
  personality engine.
- Species-specific actions (a frog's hop, a cat's pounce, a wolf's
  howl) should be expressible via this data contract, not via
  per-species C++ branches in the engine, whenever that's reasonable.

## Block 01's relationship to this spec

Block 01 did **not** implement a content loader, schema, or file
format. Its original placeholder creature
(`core::BlobSilhouette` in [`src/core/Silhouette.h`](../src/core/Silhouette.h))
was deliberately primitive — two overlapping analytic circles with one
built-in idle bob — specifically so it *didn't* imply a format
decision. It satisfied "one mandatory idle animation" and "alpha/hit
shape" conceptually, using math instead of asset files, and nothing
else in this list. It is no longer used by `src/app` (see Block 02
below) but remains a pure, tested utility —
`tests/SilhouetteTest.cpp` still exercises it directly.

Late in Block 01, the repository owner supplied a real illustrated
asset ("Bunny") as a **temporary QA fixture** to validate hit-testing
against real alpha data before the block's interactive macOS QA closed
— see `docs/DECISION_LOG.md` DEC-018 and `docs/PLATFORM_SPIKE.md` §6.
The one-off loader built for exactly that fixture
(`tools/prep_dev_sprite.py`'s `.rgba` format + `graphics::DevSprite`)
has since been retired (DEC-023) now that Block 02 supersedes it with a
general loader — its role was explicitly never to be an early version
of this spec's content-loading system.

## Block 02's relationship to this spec

Block 02 implements `content::PetDefinition` + `content::PetPackLoader`
(the "NVPACK1" binary format) — a real, working, but still
**development-only** implementation of a meaningful subset of the
fields above. See `docs/ANIMATION_RUNTIME.md` for the full design and
`docs/DECISION_LOG.md` DEC-021 through DEC-024 for why each choice was
made. Mapped against this document's field list:

| Field above | Block 02 status |
|---|---|
| stable pet id | ✅ `PetDefinition::id` |
| display name | ✅ `PetDefinition::displayName` |
| base visual size / anchor-pivot | ✅ `canvasWidth`/`canvasHeight`; per-frame `FrameDefinition::anchor` |
| one mandatory idle animation | ✅ `PetDefinition::idle`, required, ≥1 frame |
| zero or more optional animations/actions | ✅ `PetDefinition::passiveActions` (sparse, data-driven, any count) — plus one required `clickReaction`, not in this doc's original list but load-bearing for the block brief |
| atlas/frames | ✅ per-animation `frames: [FrameDefinition]`, no shared atlas yet — each frame is a full RGBA8 buffer, not atlas-packed |
| timing per frame | ✅ `AnimationDefinition::fps` or per-frame `durationMs` |
| looping | ✅ `PlaybackKind` (static / loop / one_shot) |
| alpha/hit shape | ✅ per-frame, via `core::AlphaMask::FromAlphaChannel` at `PetDefinition::alphaHitThreshold` — real per-pixel data, not analytic, and now per-*frame* (silhouette can change frame to frame) |
| purchase price in clicks | ❌ not present — no Shop in Block 02's scope |
| starter eligibility | ❌ not present — no onboarding in Block 02's scope |
| hidden/secret eligibility | ❌ not present |
| thumbnail | ❌ not present — no Shop/Collection UI to show it in |
| provenance metadata | ❌ not present |
| package/schema version | 🟡 `PetDefinition::contentVersion` exists as a schema-only string field; nothing reads or validates it yet |
| variant grouping | ✅ `PetDefinition::variantGroup` — new field, not in this doc's original list, added because the block brief anticipated future variant Nimvlets (e.g. Frin) needing a group id; schema-only, unused for selection logic in this block |

**Where future master art and its provenance will live:** see
[`assets/source/nimvlets/README.md`](../assets/source/nimvlets/README.md)
— the target directory contract (one folder per Nimvlet, `master.png` +
`animations/<category>/`) and a small per-pet `provenance.json` schema,
documented ahead of having real files so the art and engine pipelines
agree on layout before either is built further. Nothing is populated
there yet.

**Still not the final production content pipeline:** the "NVPACK1"
format is dev tooling (`tools/compile_pet_pack.py`), not a commitment to
the shipped format; no atlas packing; no real (non-derived) art for any
of the 8 Nimvlets; no content-pipeline integration (e.g. Ludo.ai); no
per-pet Shop/Collection metadata. The Bunny DEV pack
(`assets/dev/bunny_pack.nvpack`) is a QA/dev fixture generated by
deterministic pixel transforms from Block 01's Bunny QA image — see
`docs/ANIMATION_RUNTIME.md` §5 and `assets/dev/README.md` — not a
Nimvlet, not final art, not referenced by `docs/PRD_V1.md`.
