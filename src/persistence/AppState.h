#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nimvlets::persistence {

// A window's last on-screen position, in the same coordinate space
// SDL_GetWindowPosition()/SDL_SetWindowPosition() use (int, screen-space
// logical points) — see src/app/SpikeApp.cpp's drag-end handling.
struct WindowPosition {
    int x = 0;
    int y = 0;

    friend bool operator==(WindowPosition a, WindowPosition b) {
        return a.x == b.x && a.y == b.y;
    }
};

// The full set of local, on-disk application state Block 03 persists.
// Pure data — no SDL, no file I/O, no platform code (see
// AppStateSerializer.h for (de)serialization and AppStateStore.h for
// the storage policy that reads/writes it). Deliberately minimal: only
// fields with existing runtime meaning in this block are here — see
// docs/PERSISTENCE.md for what was deliberately left out and why.
//
// Generic by construction: activePetId/activeVariantId are plain
// strings, not an enum of known Nimvlets — adding a new pet id or
// variant later never requires a change to this struct, to
// AppStateSerializer, or to AppStateStore.
struct AppState {
    // Bumped only when this struct's on-disk shape changes in a way
    // that isn't backward-compatible. See AppStateSerializer.cpp for
    // how a mismatch is handled (safe defaults, never a crash, no
    // migration logic in this block).
    static constexpr std::uint32_t kCurrentSchemaVersion = 1;

    std::uint32_t schemaVersion = kCurrentSchemaVersion;

    // The only currency — see AGENTS.md §2. Starts at 0; only ever
    // incremented by a real click. uint64 so it never realistically
    // overflows.
    std::uint64_t clickBalance = 0;

    // Which pet is currently active. Empty string = no save yet /
    // unset. This block implements no pet *selection* — see
    // docs/PERSISTENCE.md — it only keeps this field truthfully in
    // sync with whichever pet the runtime actually loaded.
    std::string activePetId;

    // Which variant of activePetId is active, if that pet has variants
    // (see content::PetDefinition::variantGroup). Empty string = no
    // variant / not applicable. Nothing in this block writes a
    // non-empty value here (no variant selection exists yet) — the
    // field is carried through load/save so a future block can
    // populate it without a schema change.
    std::string activeVariantId;

    // The window's position when it was last moved by the user, so the
    // app can reopen where they left it. std::nullopt = no save yet /
    // never dragged (falls back to the existing centered-on-launch
    // default).
    std::optional<WindowPosition> lastWindowPosition;

    friend bool operator==(const AppState& a, const AppState& b) {
        return a.schemaVersion == b.schemaVersion &&
               a.clickBalance == b.clickBalance &&
               a.activePetId == b.activePetId &&
               a.activeVariantId == b.activeVariantId &&
               a.lastWindowPosition == b.lastWindowPosition;
    }
};

}  // namespace nimvlets::persistence
