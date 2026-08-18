#include "catalog/PetCatalogLoader.h"

#include <cstring>
#include <fstream>
#include <set>
#include <utility>
#include <vector>

namespace nimvlets::catalog {

namespace {

// "NVCATLG1", exactamente 8 bytes — ver docs/CATALOG.md.
constexpr char kMagic[8] = {'N', 'V', 'C', 'A', 'T', 'L', 'G', '1'};

// Cursor con verificación de límites, mismo patrón "falla en el primer
// error, recuerda solo ese" que persistence::AppStateSerializer y
// content::PetPackLoader — enteros leídos vía memcpy crudo, por lo
// tanto little-endian en la plataforma que lee (x86_64/arm64 solamente
// — ver docs/ANIMATION_RUNTIME.md).
class ByteReader {
 public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    const std::string& Error() const { return error_; }

    bool ReadBytes(void* dst, std::size_t n) {
        if (!ok_) {
            return false;
        }
        if (n > size_ - pos_) {
            Fail("unexpected end of catalog data");
            return false;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return true;
    }

    bool ReadUint8(std::uint8_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint32(std::uint32_t& out) { return ReadBytes(&out, sizeof(out)); }

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
        if (ok_) {  // conserva solo el primer fallo — los siguientes suelen ser ruido derivado de ese
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

bool LoadCatalogFromMemory(const std::uint8_t* data, std::size_t size, PetCatalog& outCatalog, std::string& outError) {
    ByteReader reader(data, size);

    char magic[8];
    if (!reader.ReadBytes(magic, sizeof(magic))) {
        outError = reader.Error();
        return false;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        outError = "not a valid Nimvlets pet catalog (bad magic)";
        return false;
    }

    std::uint32_t schemaVersion = 0;
    if (!reader.ReadUint32(schemaVersion)) {
        outError = reader.Error();
        return false;
    }
    if (schemaVersion != kCurrentCatalogSchemaVersion) {
        outError = "catalog schema version " + std::to_string(schemaVersion) +
                   " is not the supported version (" + std::to_string(kCurrentCatalogSchemaVersion) +
                   "); no migration path in this block";
        return false;
    }

    std::uint32_t entryCount = 0;
    if (!reader.ReadUint32(entryCount)) {
        outError = reader.Error();
        return false;
    }
    if (entryCount == 0) {
        outError = "catalog has zero entries; a catalog must have at least one";
        return false;
    }

    std::vector<CatalogEntry> entries;
    entries.reserve(entryCount);
    std::set<std::pair<std::string, std::string>> seenIdentities;
    std::uint32_t defaultCount = 0;

    for (std::uint32_t i = 0; i < entryCount; ++i) {
        CatalogEntry entry;
        std::uint8_t isDefaultByte = 0;
        if (!reader.ReadString(entry.identity.petId) || !reader.ReadString(entry.identity.variantId) ||
            !reader.ReadString(entry.displayName) || !reader.ReadString(entry.packPath) ||
            !reader.ReadUint8(isDefaultByte)) {
            outError = reader.Error();
            return false;
        }
        entry.isDefault = isDefaultByte != 0;

        if (entry.identity.petId.empty()) {
            outError = "catalog entry " + std::to_string(i) + ": pet id must not be empty";
            return false;
        }
        if (entry.packPath.empty()) {
            outError = "catalog entry " + std::to_string(i) + " ('" + entry.identity.petId +
                       "'): pack path must not be empty";
            return false;
        }

        const auto identityKey = std::make_pair(entry.identity.petId, entry.identity.variantId);
        if (!seenIdentities.insert(identityKey).second) {
            outError = "catalog entry " + std::to_string(i) + ": duplicate identity ('" + entry.identity.petId +
                       "', '" + entry.identity.variantId + "')";
            return false;
        }

        if (entry.isDefault) {
            ++defaultCount;
        }

        entries.push_back(std::move(entry));
    }

    if (defaultCount != 1) {
        outError = "catalog must have exactly one entry marked default, found " + std::to_string(defaultCount);
        return false;
    }

    outCatalog = PetCatalog(std::move(entries));
    return true;
}

bool LoadCatalogFromFile(const std::string& path, PetCatalog& outCatalog, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "could not open " + path;
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 0) {
        outError = "could not determine size of " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(length));
    if (!buffer.empty()) {
        file.read(reinterpret_cast<char*>(buffer.data()), length);
        if (!file) {
            outError = "failed reading " + path;
            return false;
        }
    }

    return LoadCatalogFromMemory(buffer.data(), buffer.size(), outCatalog, outError);
}

}  // namespace nimvlets::catalog
