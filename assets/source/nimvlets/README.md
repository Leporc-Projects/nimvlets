# assets/source/nimvlets — source art contract (target, not yet populated)

This directory is the **agreed destination** for real Nimvlet master art
and its animation sequences (from Krita, Ludo.ai, or any other tool),
once that art exists. **Nothing under here is populated by Block 02** —
this file documents the contract so the art pipeline and the engine
pipeline (`tools/compile_pet_pack.py` → `content::PetPackLoader`, see
`docs/ANIMATION_RUNTIME.md`) can be built to agree on layout *before*
either side has to guess, the same reasoning behind
`docs/PET_CONTENT_SPEC.md` existing before Block 01 had a content
loader at all. No per-pet subdirectories are created ahead of having
real files to put in them — see AGENTS.md's general "don't build ahead
of need" principle.

## Target layout

```
assets/source/nimvlets/
  bunny/
    master.png
    animations/
      click/
      passive/
  rato/
    master.png
    animations/
      click/
      passive/
  rin-rin/
    master.png
    animations/
      click/
      passive/
  frin/
    male/
      master.png
      animations/
        click/
        passive/
    female/
      master.png
      animations/
        click/
        passive/
  artu/
    master.png
    animations/
  kyubi/
    master.png
    animations/
  nidir/
    master.png
    animations/
  sweetie/
    master.png
    animations/
```

- One directory per Nimvlet, named with its stable id (lowercase,
  hyphenated — matching `content::PetDefinition::id`'s style, e.g.
  `"bunny_dev"` today, `"rin-rin"` for that Nimvlet later; the exact id
  string is whatever `docs/PET_CONTENT_SPEC.md`/the content team settles
  on per pet, this doc doesn't mandate one).
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
- `animations/<category>/` — one subfolder per exported PNG sequence,
  each folder holding that sequence's ordered frames (naming convention
  for individual frame files is left to whatever the export tool
  produces; `tools/compile_pet_pack.py`'s manifest references them by
  explicit filename, not by pattern-matching a folder).

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
