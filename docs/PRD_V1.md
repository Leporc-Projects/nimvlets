# Nimvlets — PRD v1 (current)

Status: living document, reflects the product as decided **now**
(2026-08-17, end of Block 01). Superseded points are marked in
`docs/DECISION_LOG.md`, not deleted here — this file always describes
current intent, not history.

## 1. What it is

A lightweight, native, cross-platform desktop companion. One small
creature ("Nimvlet") lives in a small transparent window on the user's
desktop at a time. The user can drag it and click it. Clicking earns
clicks, the game's only currency, spent permanently to unlock more
creatures.

## 2. Platforms

- v1 targets: **macOS** (Apple Silicon + Intel) and **Windows**.
- Linux is explicitly out of scope for v1.
- No embedded web runtime (Electron/Chromium/etc) — native C++ only.

## 3. Content scope

- v1 target: **8 functional Nimvlets**.
- Currently closed/named: 4 starter candidates —
  - **Artu** (cat)
  - **Rato** (rabbit)
  - **Rin Rin** (frog)
  - **Tan** (white wolf) — secret starter, see §4.
- The remaining Nimvlets for the 8-creature v1 target are **not yet
  designed**; this document will not invent names, species, or
  appearances ahead of that decision.
- No final art, audio, or branding exists yet. Block 01's spike uses an
  explicitly non-final, procedurally-drawn placeholder shape — see
  `docs/PET_CONTENT_SPEC.md`.

## 4. Starter onboarding (product decision — not yet implemented)

On first launch, the player is offered a choice of three starters:
Artu, Rato, or Rin Rin. If the choice screen goes unanswered for 44
seconds, a fourth, secret starter — Tan, a white wolf — appears as an
option. This is a structural easter egg: no assets, names, iconography,
colors, text, music, or branding from any third-party franchise are
used for it.

After picking a starter, a later, currently-unbuilt hidden area of the
Shop will let the player acquire starters they didn't originally pick.
The exact persistence/reappearance semantics for Tan (e.g., whether
missing the 44-second window is a one-time event or repeats) are **not
decided** and will be specified in a future block — do not invent this.

This entire flow is **not implemented** as of Block 01; it's recorded
here as product intent so it isn't lost, and so nothing in the codebase
accidentally contradicts it.

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

This PRD intentionally does not specify: the remaining 4+ Nimvlets
beyond the 4 starter candidates, final visual style, audio direction,
monetization beyond the click economy already described, or the
Tan-reappearance mechanic. Do not fill these gaps speculatively in code
or docs — leave them open until a block brief addresses them.
