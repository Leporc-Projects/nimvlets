# Nimvlets — Privacy & Security

## A. Standard interaction flow (implemented, Block 01)

The spike (and every future block's standard path) only ever reads
input aimed at Nimvlets itself:

- SDL window events (mouse button/motion, window close) delivered
  because the cursor is over our own window.
- The current global cursor **position** — `SDL_GetGlobalMouseState()`
  on all platforms (backed by `NSEvent.mouseLocation` polling on macOS,
  `GetCursorPos` on Windows) — polled once per idle-animation tick
  (~every 83 ms) purely to decide click-through state. This is a
  position *query*, not an event *hook*: it does not require
  Accessibility, Input Monitoring, or any other elevated permission,
  and it never sees keyboard input, other windows' content, or which
  application is focused.

No permissions beyond ordinary windowing are requested by this flow. No
Accessibility. No Input Monitoring. No Screen Recording. No admin/root.

## B. Global click mode (future, opt-in — NOT implemented)

A future, entirely separate, opt-in feature may let the user count
clicks anywhere on the system, not just on the Nimvlet. Requirements
that must hold **before** it can ship, in any block that implements it:

- The exact OS permission it needs, and exactly what it observes, must
  be explained to the user before it's requested.
- **Mouse-only.** Never keyboard.
- **No screen capture.** Ever.
- **No content inspection** (no reading what's on screen, no reading
  window titles/text).
- **No app-name tracking** (never records which application was
  focused/clicked).
- **No coordinate or click-history storage.** The only functional
  output is incrementing a counter; raw event data is never persisted.
- Implemented as a separate, clearly bounded module — not folded into
  the standard interaction path.

This is **not implemented in Block 01**, and no permission for it —
Accessibility, Input Monitoring, or otherwise — is requested anywhere
in this block's code. Grep-able guarantee: `src/` contains no reference
to `CGEventTap`, `AXIsProcessTrusted`, `SetWindowsHookEx`, or any other
global-hook API.

## C. Explicit non-goals (every block, unless one is explicitly revised)

Nimvlets does not, and this block does not add anything that:

- captures the screen or any window's contents;
- logs keyboard input, globally or otherwise;
- logs mouse clicks globally (see §B — that's a distinct, future,
  opt-in feature with its own rules);
- enumerates other running applications for behavioral purposes;
- opens network sockets or makes network requests (Block 01: zero
  network activity, confirmed in the Block 01 report);
- sends telemetry of any kind;
- downloads assets at runtime;
- requires an account or a subscription;
- writes data outside ordinary development paths (build output, the
  process's own working directory) beyond what SDL/AppKit/Win32
  themselves touch to run a window.

## D. Block 01 self-check

See the Block 01 report's "Privacy / Permissions" section for the
concrete yes/no confirmation (Accessibility / Input Monitoring / Screen
Recording / admin-root / global hooks / network — all "no") as actually
observed for this block's spike executable.
