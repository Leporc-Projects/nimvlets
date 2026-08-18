#include "persistence/AppStateStore.h"

#include "persistence/AppStateSerializer.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace nimvlets::persistence {

namespace {
constexpr const char* kStateFileName = "state.nvstate";
constexpr const char* kTempFileName = "state.nvstate.tmp";
}  // namespace

AppStateStore::AppStateStore(std::string directoryPath) : directoryPath_(std::move(directoryPath)) {}

std::string AppStateStore::StatePath() const {
    return (std::filesystem::path(directoryPath_) / kStateFileName).string();
}

std::string AppStateStore::TempPath() const {
    return (std::filesystem::path(directoryPath_) / kTempFileName).string();
}

AppState AppStateStore::Load(std::string* outWarning) const {
    std::ifstream file(StatePath(), std::ios::binary);
    if (!file) {
        if (outWarning != nullptr) {
            *outWarning = "no existing app-state save found; using defaults";
        }
        return AppState{};
    }

    // Same whole-file-into-a-buffer approach as
    // content::LoadPetPackFromFile (src/content/PetPackLoader.cpp) —
    // kept consistent rather than introducing a different idiom for
    // reading a small binary file.
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 0) {
        if (outWarning != nullptr) {
            *outWarning = "could not determine size of app-state save; using defaults";
        }
        return AppState{};
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(length));
    if (!buffer.empty()) {
        file.read(reinterpret_cast<char*>(buffer.data()), length);
        if (!file) {
            if (outWarning != nullptr) {
                *outWarning = "failed reading app-state save; using defaults";
            }
            return AppState{};
        }
    }

    AppState state;
    std::string error;
    if (!DeserializeAppState(buffer.data(), buffer.size(), state, error)) {
        if (outWarning != nullptr) {
            *outWarning = "existing app-state save could not be used (" + error + "); using defaults";
        }
        return AppState{};
    }

    if (outWarning != nullptr) {
        outWarning->clear();
    }
    return state;
}

bool AppStateStore::Save(const AppState& state, std::string& outError) const {
    const std::vector<std::uint8_t> bytes = SerializeAppState(state);
    const std::string tempPath = TempPath();

    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            outError = "could not open temp file for writing: " + tempPath;
            return false;
        }
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            outError = "failed writing app state to temp file: " + tempPath;
            return false;
        }
        // `file` is flushed and closed here (end of scope), before the
        // rename below — the rename must never race an open handle.
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, StatePath(), ec);
    if (ec) {
        outError = "failed to atomically replace app-state file: " + ec.message();
        return false;
    }

    return true;
}

}  // namespace nimvlets::persistence
