#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "persistence/AppState.h"

namespace nimvlets::persistence {

// Deterministic, versioned binary (de)serialization for AppState — see
// docs/PERSISTENCE.md for the exact on-disk layout ("NVSTATE1"). Pure:
// no file I/O here (see AppStateStore.h for that), so this is directly
// unit-testable with in-memory byte buffers — the same separation
// content::PetPackLoader established in Block 02 between parsing and
// file access.

// Deterministic: calling this twice on an unchanged AppState produces
// byte-identical output — no timestamps, no padding, no map/set
// iteration order anywhere in the format.
std::vector<std::uint8_t> SerializeAppState(const AppState& state);

// Fails loudly (returns false, `outError` set to a specific message)
// on a bad magic value, truncated data, or a schemaVersion that
// doesn't exactly match AppState::kCurrentSchemaVersion — this block
// has no migration path for an older/newer schema, so any mismatch is
// treated as "cannot use this data" rather than guessed at. `outState`
// is left in an unspecified state on failure; always check the return
// value.
bool DeserializeAppState(const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError);

}  // namespace nimvlets::persistence
