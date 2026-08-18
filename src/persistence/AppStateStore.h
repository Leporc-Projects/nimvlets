#pragma once

#include <string>

#include "persistence/AppState.h"

namespace nimvlets::persistence {

// Reads/writes one AppState to a single file inside `directoryPath`,
// with an atomic-write strategy (write to a temp file, then rename
// over the real one) so a save interrupted partway (crash, power loss,
// disk full) can never leave a half-written, corrupt file in place of
// a previously good one. See docs/PERSISTENCE.md for the exact
// file-name/path policy.
//
// This class assumes `directoryPath` already exists — in production,
// SDL_GetPrefPath() guarantees this (it creates the directory itself);
// tests must create their own temporary directory before constructing
// this class. It never resolves a path itself and never touches
// anything outside the one file (plus its `.tmp` staging file) inside
// `directoryPath` — see tests/AppStateStoreTest.cpp, which points it
// at fresh, isolated temp directories, never the user's real app-data
// location.
class AppStateStore {
 public:
    explicit AppStateStore(std::string directoryPath);

    // Returns a previously saved state, or AppState{} (safe defaults)
    // if no save exists yet, the file can't be read, or its contents
    // don't parse (bad magic, truncated, unsupported schema version).
    // Never throws, never crashes the caller. If `outWarning` is
    // non-null, it's set to a short, specific, human-readable reason
    // whenever the result isn't "a valid, current-schema save was
    // found" (and cleared otherwise) — the caller decides whether/how
    // to log it.
    AppState Load(std::string* outWarning = nullptr) const;

    // Serializes `state` and writes it atomically: the full contents
    // are first written to a temp file in the same directory (so the
    // final rename is on the same filesystem, which is what makes it
    // atomic), and only renamed into place if that write fully
    // succeeded. If anything fails — the temp file can't be created or
    // written, or the rename fails — the previously saved file (if
    // any) is left completely untouched, and this returns false with
    // `outError` set to a specific reason. Never throws, never
    // crashes.
    bool Save(const AppState& state, std::string& outError) const;

 private:
    std::string StatePath() const;
    std::string TempPath() const;

    std::string directoryPath_;
};

}  // namespace nimvlets::persistence
