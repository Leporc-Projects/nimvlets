#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "content/AnimationDefinition.h"

namespace nimvlets::content {

// Parses a compiled pet pack (see tools/compile_pet_pack.py for the
// producer and docs/ANIMATION_RUNTIME.md for the exact on-disk format,
// "NVPACK1") from an in-memory byte buffer. Pure parsing — no file I/O
// — so it's directly unit-testable with small synthetic buffers (see
// tests/PetPackLoaderTest.cpp) without any filesystem/CWD dependency,
// the same lesson Block 01's DevSprite deliberately avoided testing
// this way and Block 02 fixes by design.
//
// Fails loudly: returns false and a human-readable message in
// `outError` on any structural problem (bad magic, truncated data,
// frame dimensions that differ from the first frame within one
// animation, an animation with zero frames) — never silently invents,
// clamps, or truncates data. `outPet` is left in an unspecified state
// on failure; always check the return value.
bool LoadPetPackFromMemory(const std::uint8_t* data, std::size_t size, PetDefinition& outPet, std::string& outError);

// Reads `path` into memory and calls LoadPetPackFromMemory(). Returns
// false (with `outError` set) if the file can't be opened/read at all,
// distinct from — but reported the same way as — a malformed-content
// failure; callers only need to check the one boolean either way.
bool LoadPetPackFromFile(const std::string& path, PetDefinition& outPet, std::string& outError);

}  // namespace nimvlets::content
