#include "productui/PreviewArtifact.h"

#include <cstring>
#include <fstream>

namespace nimvlets::productui {

namespace {

// "NVPREV1" + NUL, exactamente 8 bytes.
constexpr char kMagic[8] = {'N', 'V', 'P', 'R', 'E', 'V', '1', '\0'};
constexpr std::uint32_t kSupportedVersion = 1;

// Cursor con verificación de límites — mismo contrato "falla en el
// primer error, recuerda solo ese" que content::PetPackLoader y
// catalog::PetCatalogLoader. Enteros little-endian vía memcpy (todas las
// plataformas objetivo son little-endian).
class ByteReader {
 public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    bool ok() const { return ok_; }
    const std::string& Error() const { return error_; }
    std::size_t Remaining() const { return size_ - pos_; }

    bool ReadBytes(void* dst, std::size_t n) {
        if (!ok_) {
            return false;
        }
        if (n > size_ - pos_) {
            Fail("unexpected end of preview data");
            return false;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return true;
    }

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
        if (ok_) {
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

bool LoadPetPreviewFromMemory(
    const std::uint8_t* data, std::size_t size, PetPreviewImage& outImage, std::string& outError) {
    ByteReader reader(data, size);

    char magic[8];
    if (!reader.ReadBytes(magic, sizeof(magic))) {
        outError = reader.Error();
        return false;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        outError = "not a valid Product UI preview (bad magic — expected NVPREV1)";
        return false;
    }

    std::uint32_t version = 0;
    if (!reader.ReadUint32(version)) {
        outError = reader.Error();
        return false;
    }
    if (version != kSupportedVersion) {
        outError = "preview version " + std::to_string(version) + " is not supported (this build reads 1)";
        return false;
    }

    PetPreviewImage image;
    if (!reader.ReadString(image.petId) || !reader.ReadString(image.variantId) ||
        !reader.ReadString(image.sourcePack)) {
        outError = reader.Error();
        return false;
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t pixelBytes = 0;
    if (!reader.ReadUint32(width) || !reader.ReadUint32(height) || !reader.ReadUint32(pixelBytes)) {
        outError = reader.Error();
        return false;
    }
    if (width == 0 || height == 0) {
        outError = "preview has non-positive dimensions";
        return false;
    }

    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (pixelBytes != expected) {
        outError = "preview pixel_bytes " + std::to_string(pixelBytes) + " does not match " +
                   std::to_string(width) + "x" + std::to_string(height) + "x4 (" + std::to_string(expected) + ")";
        return false;
    }
    if (reader.Remaining() < pixelBytes) {
        outError = "preview is truncated: " + std::to_string(reader.Remaining()) + " bytes left, need " +
                   std::to_string(pixelBytes);
        return false;
    }

    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.rgba.resize(pixelBytes);
    if (!reader.ReadBytes(image.rgba.data(), pixelBytes)) {
        outError = reader.Error();
        return false;
    }

    outImage = std::move(image);
    return true;
}

bool LoadPetPreviewFromFile(const std::string& path, PetPreviewImage& outImage, std::string& outError) {
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

    return LoadPetPreviewFromMemory(buffer.data(), buffer.size(), outImage, outError);
}

std::string PreviewPathForPack(const std::string& packPath) {
    constexpr const char* kPackExt = ".nvpack";
    constexpr std::size_t kPackExtLen = 7;
    if (packPath.size() >= kPackExtLen &&
        packPath.compare(packPath.size() - kPackExtLen, kPackExtLen, kPackExt) == 0) {
        return packPath.substr(0, packPath.size() - kPackExtLen) + ".nvprev";
    }
    return packPath + ".nvprev";
}

}  // namespace nimvlets::productui
