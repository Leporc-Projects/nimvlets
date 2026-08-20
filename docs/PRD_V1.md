# Nimvlets — PRD v1 (current)

Status: living document, reflects the product as decided **now**
(2026-08-19, Block 05). Superseded points are marked in
`docs/DECISION_LOG.md`, not deleted here — this file always describes
current intent, not history.

## 1. What it is

A lightweight, native, cross-platform desktop companion. One small
creature ("Nimvlet") lives in a small transparent window on the user's
desktop at a time. The user can drag it and click it. Clicking earns
clicks, the game's only currency, spent permanently to unlock more
creatures.

## 2. Platforms

- v1 targets: **macOS** (Apple Silicon + Intel, universal2), **Windows**,
  and **Linux x86_64** (X11 and Wayland — enabled since Block 04.1; see
  `docs/LINUX_PLATFORM.md` for the platform-specific findings and known
  Wayland limitations, e.g. no client-requestable always-on-top or
  absolute window positioning under `xdg-shell`).
- No embedded web runtime (Electron/Chromium/etc) — native C++ only.

## 3. Content scope

- v1 target: **8 functional Nimvlets**.
- Current known logical v1 set (see `docs/DECISION_LOG.md` for when
  each was named/scoped):
  - **Bunny** (rabbit) — real production art since Block 04.3.
  - **Rato** — not yet populated with real art.
  - **Rin Rin** — not yet populated with real art.
  - **Frin** (white/cream wolf) — real production art since Block 05;
    ONE logical Nimvlet with TWO visual variants, male and female (see
    `docs/CATALOG.md` and `docs/FRIN_CONTENT.md`) — secret starter, see
    §4. Previously referred to as "Tan" in earlier drafts of this
    document; renamed to match the owner's actual final content.
  - **Artu** (cat) — not yet populated with real art; a future block's
    stateful behavior (seated/sit-to-lie/lying/lie-to-sit, 70%
    belly-roll / 30% stretch) is architecturally compatible with the
    named-state behavior graph Frin introduced (see
    `docs/ANIMATION_RUNTIME.md`) — no engine change anticipated.
  - **Nidir** (dragon) — real production art since Block 04.2; NOT a
    starter candidate, a normal unlockable Nimvlet.
  - **Kyubi** — not yet populated with real art.
  - **Sweetie** — not yet populated with real art.
- No final audio or branding exists yet.

## 4. Starter onboarding (product decision — not yet implemented)

On first launch, the player is offered a choice of three starters:
Artu, Rato, or Rin Rin. If the choice screen goes unanswered for 44
seconds, a fourth, secret starter — **Frin**, a white/cream wolf —
appears as an option. This is a structural easter egg: no assets,
names, iconography, colors, text, music, or branding from any
third-party franchise are used for it. Which visual variant (male/
female) appears for the secret starter is not decided — see
`docs/CATALOG.md` for how the catalog already models Frin as one
`pet_id` with two `variant_id`s so that decision doesn't require an
engine change either way.

After picking a starter, a later, currently-unbuilt hidden area of the
Shop will let the player acquire starters they didn't originally pick.
The exact persistence/reappearance semantics for Frin (e.g., whether
missing the 44-second window is a one-time event or repeats) are **not
decided** and will be specified in a future block — do not invent this.

This entire flow is **not implemented** as of this block; it's
recorded here as product intent so it isn't lost, and so nothing in
the codebase accidentally contradicts it. No onboarding/shop UI exists
yet — pets are only reachable via the `NIMVLETS_DEV_SELECT_PET`/
`NIMVLETS_DEV_SWITCH_TEST_COUNT` dev-only mechanisms (see README.md).

## 5. Economy

- Clicks are the **only** currency.
- Starting balance: **0** clicks.
- Unlocking/purchasing a Nimvlet subtracts its price from the balance.
- A purchased Nimvlet is owned **permanently**.
- Switching between owned Nimvlets costs nothing.
- No serious anti-cheat: autoclickers are explicitly not a product
  concern.
- No notification is shown when clicks are earned, in the current
  version.
- Shop (buy Nimvlets) and Collection (view/switch owned Nimvlets) are
  **separate** areas of the eventual UI.

None of Shop, Collection, or persisted economy exist yet — Block 01's
spike counts clicks in memory only, logged to stdout for development
visibility, and discards them on exit.

## 6. Global click mode (future, opt-in — not implemented)

A future, fully **opt-in** mode may count clicks anywhere on the
system, not just on the Nimvlet itself. When it exists, it must:

- clearly explain what OS permission it needs and exactly what it
  observes, before requesting anything;
- be mouse-only — never keyboard;
- never do screen capture or content/text inspection;
- never track which app is focused/named;
- never store coordinate or click-history data — only increment a
  counter.

This is not implemented, and no permission for it is requested, in
Block 01 or any block before one that explicitly authorizes it. See
`docs/PRIVACY_SECURITY.md`.

## 7. Fullscreen presence (future, configurable — not implemented)

A future version will let the user decide whether the pet appears over
fullscreen applications. Not decided or implemented yet; Block 01's
spike does not request fullscreen presence.

## 8. Non-functional requirements

- **Local-first, offline-capable.** No required account.
- **Privacy-respecting by default**: no behavioral telemetry, no screen
  capture, no reading of personal files, no keylogging, no ads, no
  mandatory subscription. See `docs/PRIVACY_SECURITY.md`.
- **Efficient**: small RAM/CPU/binary footprint is a product
  requirement, not an afterthought. See `docs/PERFORMANCE_BUDGETS.md`.
- **Event-driven**: no permanent fixed-rate game loop when nothing is
  changing.

## 9. Explicitly not decided yet

This PRD intentionally does not specify: final art for Rato, Rin Rin,
Artu, Kyubi, or Sweetie, final visual style, audio direction,
monetization beyond the click economy already described, or the
Frin-reappearance mechanic. Do not fill these gaps speculatively in
code or docs — leave them open until a block brief addresses them.
