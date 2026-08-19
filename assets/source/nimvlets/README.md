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
relate.

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
  bunny/           -- not populated (Bunny stays a Block 01 QA fixture under assets/dev/, not here)
  rato/            -- not populated yet
  rin-rin/         -- not populated yet
  frin/
    male/          -- not populated yet
    female/        -- not populated yet
  artu/            -- not populated yet
  kyubi/           -- not populated yet
  sweetie/         -- not populated yet
```

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
- `master.png` — the single reference illustration a pet's animation
  frames are derived from (hand-drawn, AI-generated, or both);
  intentionally unopinionated about resolution or tool.
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
  pet, living next to its own source frames (unlike Bunny's manifest,
  which lives under `assets/dev/bunny_pack/` since Bunny's frames are
  themselves dev-generated, not a hand-authored source tree).

## `click` / `passive` are categories, not a hard limit

`click` and `passive` are this block's two required action categories —
they map directly to `content::PetDefinition::clickReaction` (one
required reaction) and `content::PetDefinition::passiveActions` (an
already-arbitrary-length, data-driven list — a pet can have one passive
action or a dozen, entirely through pack data, with zero engine-code
changes; see `docs/ANIMATION_RUNTIME.md` §2). Adding a *new* passive
action (e.g. `passive/yawn/` alongside `passive/wiggle/`) needs no
runtime change at all today.

A pet needing an action category beyond click/passive (say, a
drag-reaction or a hover-idle-variant) is a real but *future* need —
`PetDefinition` would gain one small, additive field for it (following
the same pattern `passiveActions` already establishes) when a block
actually requires it. Nothing here should be read as a promise that
only these two categories will ever exist, and nothing here pre-builds
that generalization speculatively before a real consumer needs it.

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

**Illustrative example** (not a real content record — Bunny is a QA
fixture living in `assets/dev/`, not a Nimvlet, and is not stored under
this contract; these are simply Bunny's own real, already-documented
facts — see `docs/DECISION_LOG.md` DEC-018 — reused here because they're
the one real, truthful example this repository has, not because Bunny
belongs under `assets/source/nimvlets/`):

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
