# assets/source/nimvlets — source art contract

This directory is the **agreed destination** for real Nimvlet master art
and its animation sequences (from Krita, Ludo.ai, or any other tool).
This file documents the contract so the art pipeline and the engine
pipeline (`tools/compile_pet_pack.py` → `content::PetPackLoader`, see
`docs/ANIMATION_RUNTIME.md`) agree on layout. No per-pet subdirectories
are created ahead of having real files to put in them — see AGENTS.md's
general "don't build ahead of need" principle.

**Block 04.2 populated the first real entry** (`nidir/`, see
`docs/NIDIR_CONTENT.md`) and, with it, refined the target layout below
from Block 02's original sketch: individual PNG frames (not a
spritesheet) are the canonical animation source, directional
(`right`/`left`) asset sets are explicit subfolders, and
`DESCRIPTION.txt` (not `provenance.json`) is what a real pet actually
ships with today — see "Provenance record" below for how the two
relate. **Block 04.3 populated the second real entry** (`bunny/`, see
`docs/BUNNY_CONTENT.md`) — Bunny's real production art (idle + click)
migrated it off the Block 01/02 QA-fixture path described lower in
this file. **Block 05 populated the third and fourth real entries**
(`frin/male/`, `frin/female/` — one logical Nimvlet, two visual
variants, see the block's report) and generalized the runtime content
model from a fixed idle/click/passive shape to a named-state behavior
graph (`content::BehaviorState`/`WeightedAction`, "NVPACK2" — see
`docs/ANIMATION_RUNTIME.md`); `pack_manifest.json`'s shape changed
accordingly (`states: [...]` instead of `idle`/`click_reaction`/
`passive_actions` at the top level — see "Manifest shape" below).
Nidir/Bunny's own manifests were regenerated to the new shape (one
`"default"` state each) with no change to their source PNGs.

## Target layout

```
assets/source/nimvlets/
  nidir/                          -- Block 04.2, real, see docs/NIDIR_CONTENT.md
    DESCRIPTION.txt
    master.png
    pack_manifest.json
    animations/
      idle/
        right/
          frames/frame_000.png .. frame_NNN.png   -- canonical source
          spritesheet/spritesheet.png              -- secondary export/reference
        left/
          frames/frame_000.png .. frame_NNN.png   -- deterministic horizontal mirror
          spritesheet/spritesheet.png              -- assembled from the mirrored frames
  bunny/                          -- Block 04.3, real, see docs/BUNNY_CONTENT.md
    DESCRIPTION.txt
    master.png
    pack_manifest.json
    animations/
      idle/
        left/    -- canonical direction for THIS export (owner-named; not the same as Nidir's "right")
          frames/frame_000.png .. frame_NNN.png
          spritesheet/spritesheet.png
        right/   -- deterministic horizontal mirror
          frames/frame_000.png .. frame_NNN.png
          spritesheet/spritesheet.png
      click_reaction/
        left/ ... right/          -- same shape as idle
  frin/                            -- Block 05, real, one logical Nimvlet, two variants
    male/                          -- canonical direction LEFT (owner's export)
      DESCRIPTION.txt
      master.png
      pack_manifest.json           -- states: [seated, lying] -- see "Manifest shape" below
      animations/
        sit_to_lie/  left/ ... right/   -- seated -> lying transition (ambient in "seated")
        lie_to_sit/  left/ ... right/   -- lying -> seated transition (click in "lying")
        howl/        left/ ... right/   -- seated click, weight 0.7
        tail_greet/  left/ ... right/   -- seated click, weight 0.3
    female/                        -- canonical direction RIGHT (owner's export) -- same 4
                                       animations, same state ids, same weights; only the
                                       canonical/derived direction assignment is mirrored.
  rato/            -- not populated yet
  artu/            -- not populated yet (architecture-compatible: same seated/lying
                      state-graph shape Frin uses today would fit Artu's future
                      seated/sit-to-lie/lying/lie-to-sit + 70% belly-roll / 30% stretch
                      without any engine change — see the Block 05 report)
  kyubi/           -- not populated yet
  sweetie/         -- not populated yet
```

## Manifest shape (Block 05 — named-state behavior graph)

`pack_manifest.json` no longer has top-level `idle`/`click_reaction`/
`passive_actions` fields (Block 02-04.3's shape). Every pet — normal or
stateful — now has a `"states"` array, each entry a `content::
BehaviorState`: a `base_animation` (the pose shown at rest) plus three
weighted-action triggers (`ambient_actions`, `hover_actions`,
`click_actions`), each action naming a `target_state_id` to transition
to when it finishes. A normal pet (Bunny, Nidir) is exactly one state
("default") whose actions all self-loop back to it — the *same* shape
a stateful pet (Frin) uses, just with one state instead of several and
every transition targeting itself. See `docs/ANIMATION_RUNTIME.md` for
the full field-by-field contract and `tools/compile_pet_pack.py`'s
module docstring for the exact JSON schema, and
`tools/generate_frin_pack.py` for a real multi-state manifest builder
next to `tools/generate_bunny_pack.py`/`generate_nidir_pack.py` for the
single-state case.

- One directory per Nimvlet, named with its stable id (lowercase,
  hyphenated — matching `content::PetDefinition::id`'s style, e.g.
  `"nidir"`; the exact id string is whatever `docs/PET_CONTENT_SPEC.md`/
  the content team settles on per pet, this doc doesn't mandate one).
- **Variants** (e.g. Frin's male/female) nest *above* `master.png` —
  `frin/male/master.png`, `frin/female/master.png` — not as a suffix on
  a shared master, since each variant is visually distinct end to end.
  This mirrors `content::PetDefinition::variantGroup` (schema-only today
  — see `docs/PET_CONTENT_SPEC.md`), which is how the engine will later
  know `frin/male` and `frin/female` are two variants of one logical
  Nimvlet rather than two unrelated pets.
- `master.png` — **(Block 05 policy)** a deterministic, real, RGBA copy
  of frame 0 of the pet's canonical base pose, in its canonical
  direction (e.g. Bunny: `animations/idle/left/frames/frame_000.png`;
  Frin male: `animations/sit_to_lie/left/frames/frame_000.png`) —
  written by each `generate_<pet>_pack.py` via
  `prep_dev_sprite.write_master_from_canonical_frame()`, never edited
  by hand. Earlier blocks treated `master.png` as a separate "hero
  shot" reference asset (a different render/crop than the actual
  animation frames, sometimes without even a real alpha channel); that
  turned out to be misleading (never actually used by the compile
  pipeline, but inconsistent with what the pet actually looks like
  on-screen) — see `docs/DECISION_LOG.md` DEC-073. Regenerate it by
  re-running that pet's `generate_<pet>_pack.py` after changing its
  source frames.
- `DESCRIPTION.txt` — stable physical traits (in Spanish), for keeping
  future generations/edits visually consistent — see
  `docs/NIDIR_CONTENT.md` §4 and `nidir/DESCRIPTION.txt` for a real
  example. Not a product/personality spec (that's
  `docs/PET_CONTENT_SPEC.md`/`docs/PRD_V1.md`).
- `animations/<name>/<direction>/frames/` — **canonical source**: one
  subfolder per exported PNG sequence, ordered, deterministically named
  `frame_000.png`, `frame_001.png`, ... `tools/validate_frame_sequence.py`
  (Block 04.2) checks this contract (dimensions, ordering, no gaps/
  duplicates, real non-degenerate alpha) before anything compiles them.
  `<direction>` (`right`/`left`) only exists for animations that
  actually have direction-specific art — see
  `docs/NIDIR_CONTENT.md` §5 for how a non-directional animation (or a
  whole non-directional pet) simply omits it.
- `animations/<name>/<direction>/spritesheet/` — **secondary
  artifact/reference only**, never what `tools/compile_pet_pack.py`
  reads. If the spritesheet and the individual frames ever disagree,
  the frames win.
- `pack_manifest.json` — the `tools/compile_pet_pack.py` input for this
  pet, living next to its own source frames. Bunny/Nidir/Frin's
  manifests all follow the Block 05 named-state shape (see "Manifest
  shape" above); the OLD synthetic dev-fixture manifest
  (`assets/dev/bunny_pack/manifest.json`) and its generator
  (`tools/generate_bunny_dev_pack.py`) were removed in Block 05 —
  superseded since Block 04.3 (Bunny has had real production art since
  then), and no longer even loadable once the runtime format moved to
  "NVPACK2" (see `docs/DECISION_LOG.md`).

## Ambient / hover / click are triggers, not a hard limit

Every `content::BehaviorState` has exactly three trigger kinds today —
`ambient_actions` (timer-driven), `hover_actions` (continuous-dwell-driven
— see `core::HoverDwellTracker`/`docs/ANIMATION_RUNTIME.md` §8.1 —
often just reusing the ambient pool via `hover_uses_ambient_actions`),
and `click_actions` (click-driven) — each an already-arbitrary-length,
data-driven weighted list (a state can have one action or a dozen under
any trigger, entirely through pack data, zero engine-code changes; see
`docs/ANIMATION_RUNTIME.md`). Adding a *new* weighted action to an
existing trigger (or a new `BehaviorState` with its own transitions,
the way Frin's `seated`/`lying` do) needs no runtime change at all
today.

A pet needing a trigger kind beyond ambient/hover/click (say, a
drag-reaction) is a real but *future* need — `BehaviorState` would gain
one small, additive field for it when a block actually requires it.
Nothing here should be read as a promise that only these three trigger
kinds will ever exist, and nothing here pre-builds that generalization
speculatively before a real consumer needs it.

## Provenance record

**Not yet instantiated for Nidir** (Block 04.2 shipped `DESCRIPTION.txt`
instead — physical-consistency traits, not origin tracking; see
`docs/NIDIR_CONTENT.md` §7). The two serve different purposes and can
coexist: `DESCRIPTION.txt` answers "what must never change by
accident," `provenance.json` below answers "where did this come from."
Nothing here contradicts adding a real `provenance.json` for Nidir (or
any pet) in a future block.

Alongside each pet's `master.png`, a small `provenance.json` should
record where the art came from — not a database, not a legal system,
just enough for the team to answer "where did this come from and can we
prove it" later. One file per pet (or per variant, for Frin), living
next to the `master.png` it describes:

```
assets/source/nimvlets/<pet>/provenance.json
assets/source/nimvlets/frin/male/provenance.json   (per-variant, for Frin)
```

| Field | Type | Notes |
|---|---|---|
| `nimvlet_id` | string | Matches `content::PetDefinition::id` once compiled. |
| `display_name` | string | Matches `content::PetDefinition::displayName`. |
| `variant` | string | Empty/omitted if this pet has no variants; e.g. `"male"` for Frin. |
| `master_file` | string | Relative filename, e.g. `"master.png"`. |
| `source_origin` | string | Free text — how the art was produced (commissioned illustration, AI-generated via a named tool, a reference photo transformed by hand, ...). |
| `date` | string | ISO 8601 (`YYYY-MM-DD`). |
| `tools_used` | array of strings | e.g. `["Krita"]`, `["Ludo.ai", "Krita"]`. |
| `transformation_notes` | string | What was done to get from the raw source to `master.png` (resize, cleanup, alpha fix, ...). Empty string if none. |
| `alpha_hit_threshold_override` | integer or `null` | Only set when this pet needs a different hit-test threshold than `PetDefinition::alphaHitThreshold`'s default (128) — see `docs/DECISION_LOG.md` DEC-018 for why a threshold might need tuning per asset. `null`/omitted means "use the default." |

**Illustrative example, historical** (Bunny's ORIGINAL Block 01 QA
fixture, before Block 04.3 migrated it to real production art living
here under `assets/source/nimvlets/bunny/` — kept as the illustrative
example since it's a real, already-documented fact, not because the
`bunny_dev` id or these specific values are current; `bunny/`'s CURRENT
real content has no `provenance.json` either, same as Nidir's, since
no pet has one yet — see `alpha_hit_threshold_override` below for the
one place per-pet tuning is expected to matter):

```json
{
  "nimvlet_id": "bunny_dev",
  "display_name": "Bunny (dev fixture)",
  "variant": "",
  "master_file": "master.png",
  "source_origin": "Owner-supplied illustration, used as a QA fixture only (not final Nimvlet art)",
  "date": "2026-08-10",
  "tools_used": ["sips"],
  "transformation_notes": "Resized from 1254x1254 to 320x320 via `sips`, alpha channel preserved, no other edits.",
  "alpha_hit_threshold_override": null
}
```

No provenance files exist yet under this directory — this section is
the schema new ones should follow, written down before the first real
one is added.
