# Nimvlets — Pet Content Spec (initial contract)

This describes the shape a Nimvlet's content will eventually take —
**not** a final file format, and not implemented as a loader in Block
01. It exists so future content work and future engine work agree on
vocabulary before either is built. Nothing here is committed to a
specific serialization (JSON vs. a binary atlas format vs. something
else) — that's a future decision.

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

Block 01 does **not** implement a content loader, schema, or file
format. Its original placeholder creature
(`core::BlobSilhouette` in [`src/core/Silhouette.h`](../src/core/Silhouette.h))
is deliberately primitive — two overlapping analytic circles with one
built-in idle bob — specifically so it *doesn't* imply a format
decision. It satisfies "one mandatory idle animation" and "alpha/hit
shape" conceptually, using math instead of asset files, and nothing
else in this list. It remains the spike's fallback visual and is still
what `tests/SilhouetteTest.cpp` exercises directly.

Late in Block 01, the repository owner supplied a real illustrated
asset ("Bunny") as a **temporary QA fixture** to validate hit-testing
against real alpha data before the block's interactive macOS QA closed
— see `docs/DECISION_LOG.md` DEC-018 and `docs/PLATFORM_SPIKE.md` §6.
`tools/prep_dev_sprite.py` + `graphics::DevSprite` is a minimal,
one-off loader for exactly that fixture (a fixed uncompressed RGBA
format, no atlas, no animation frames, no schema version, no
provenance metadata) — it is explicitly **not** an early version of
this spec's content-loading system, does not commit to any of the
fields or formats above, and should not be extended or assumed to
generalize by a future block. The real content loader described by
this document remains unimplemented.
