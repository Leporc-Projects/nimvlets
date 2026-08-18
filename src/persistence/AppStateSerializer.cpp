#include "persistence/AppStateSerializer.h"

#include <cstring>
#include <utility>

namespace nimvlets::persistence {

namespace {

// "NVSTATE1", exactly 8 bytes — see docs/PERSISTENCE.md.
constexpr char kMagic[8] = {'N', 'V', 'S', 'T', 'A', 'T', 'E', '1'};

void AppendUint8(std::vector<std::uint8_t>& out, std::uint8_t v) {
    out.push_back(v);
}

void AppendUint32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendUint64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    std::uint8_t bytes[8];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendInt32(std::vector<std::uint8_t>& out, std::int32_t v) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendString(std::vector<std::uint8_t>& out, const std::string& s) {
    AppendUint32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

// Bounds-checked cursor, same "fail on first error, remember only that
// one" discipline as content::PetPackLoader's ByteReader (see
// src/content/PetPackLoader.cpp) — integers are read via a raw memcpy
// and are therefore little-endian on the reading platform, matching
// the Append* functions above and every other on-disk format in this
// repository (x86_64/arm64 only — see docs/ANIMATION_RUNTIME.md).
class ByteReader {
 public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    const std::string& Error() const { return error_; }

    bool ReadBytes(void* dst, std::size_t n) {
        if (!ok_) {
            return false;
        }
        if (n > size_ - pos_) {
            Fail("unexpected end of app-state data");
            return false;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return true;
    }

    bool ReadUint8(std::uint8_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint32(std::uint32_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint64(std::uint64_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadInt32(std::int32_t& out) { return ReadBytes(&out, sizeof(out)); }

    bool ReadString(std::string& out) {
        std::uint32_t len = 0;
        if (!ReadUint32(len)) {
            return false;
        }
        out.assign(static_cast<std::size_t>(len), '\0');
        if (len == 0) {
            return true;
        }
        return ReadBytes(out.data(), len);
    }

    void Fail(const std::string& message) {
        if (ok_) {  // keep only the first failure — later ones are usually noise from it
            ok_ = false;
            error_ = message;
        }
    }

 private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    bool ok_ = true;
    std::string error_;
};

}  // namespace

std::vector<std::uint8_t> SerializeAppState(const AppState& state) {
    std::vector<std::uint8_t> out;
    out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
    AppendUint32(out, state.schemaVersion);
    AppendUint64(out, state.clickBalance);
    AppendString(out, state.activePetId);
    AppendString(out, state.activeVariantId);
    AppendUint8(out, state.lastWindowPosition.has_value() ? 1 : 0);
    AppendInt32(out, state.lastWindowPosition ? state.lastWindowPosition->x : 0);
    AppendInt32(out, state.lastWindowPosition ? state.lastWindowPosition->y : 0);
    return out;
}

bool DeserializeAppState(const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError) {
    ByteReader reader(data, size);

    char magic[8];
    if (!reader.ReadBytes(magic, sizeof(magic))) {
        outError = reader.Error();
        return false;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        outError = "not a valid Nimvlets app-state file (bad magic)";
        return false;
    }

    AppState state;
    if (!reader.ReadUint32(state.schemaVersion)) {
        outError = reader.Error();
        return false;
    }
    if (state.schemaVersion != AppState::kCurrentSchemaVersion) {
        outError = "app-state schema version " + std::to_string(state.schemaVersion) +
                   " is not the supported version (" + std::to_string(AppState::kCurrentSchemaVersion) +
                   "); no migration path in this block";
        return false;
    }

    if (!reader.ReadUint64(state.clickBalance) || !reader.ReadString(state.activePetId) ||
        !reader.ReadString(state.activeVariantId)) {
        outError = reader.Error();
        return false;
    }

    std::uint8_t hasPosition = 0;
    std::int32_t posX = 0;
    std::int32_t posY = 0;
    if (!reader.ReadUint8(hasPosition) || !reader.ReadInt32(posX) || !reader.ReadInt32(posY)) {
        outError = reader.Error();
        return false;
    }
    if (hasPosition != 0) {
        state.lastWindowPosition = WindowPosition{posX, posY};
    }

    outState = std::move(state);
    return true;
}

}  // namespace nimvlets::persistence
