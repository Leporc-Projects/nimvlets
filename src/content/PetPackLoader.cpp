#include "content/PetPackLoader.h"

#include <cstring>
#include <fstream>
#include <utility>

namespace nimvlets::content {

namespace {

// "NVPACK1" + a trailing NUL, exactly 8 bytes — see
// docs/ANIMATION_RUNTIME.md for the full format this reader implements.
constexpr char kMagic[8] = {'N', 'V', 'P', 'A', 'C', 'K', '1', '\0'};

// Bounds-checked cursor over an in-memory byte buffer. Every Read*
// method fails (and remembers the *first* failure reason) rather than
// reading past the end — this is what makes "fail loudly on
// malformed/truncated data" hold for every field, not just the ones a
// caller remembered to check by hand.
//
// Integers/floats are read via a raw memcpy and are therefore
// little-endian on the reading platform, matching how
// tools/compile_pet_pack.py writes them (Python's `struct.pack("<...")`).
// Every platform this project targets (x86_64, arm64) is little-endian,
// so no byte-swapping is implemented — the same call made for Block 01's
// DevSprite raw format.
class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    bool ok() const { return ok_; }
    const std::string& Error() const { return error_; }

    bool ReadBytes(void* dst, std::size_t n) {
        if (!ok_) {
            return false;
        }
        if (n > size_ - pos_) {
            Fail("unexpected end of pack data");
            return false;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return true;
    }

    bool ReadUint8(std::uint8_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint32(std::uint32_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadFloat64(double& out) { return ReadBytes(&out, sizeof(out)); }

    // True si queda al menos un byte sin leer. Usado únicamente para
    // distinguir, sin ambigüedad, un pack "NVPACK1" pre-Block-04.2 (que
    // termina exactamente después de passiveActions, sin ningún byte
    // más) de uno que sí incluye la sección final opcional de
    // idleDirectionOverrides (Block 04.2) — ver
    // tools/compile_pet_pack.py, que solo escribe esa sección cuando el
    // manifest la pide explícitamente. Nunca se usa para nada más: cada
    // campo dentro de esa sección, una vez que se sabe que existe,
    // sigue siendo obligatorio y falla ruidosamente igual que el resto
    // del formato si está truncado.
    bool HasMoreData() const { return pos_ < size_; }

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

    bool ReadPixels(std::vector<std::uint8_t>& out, std::size_t n) {
        out.assign(n, 0);
        if (n == 0) {
            return true;
        }
        return ReadBytes(out.data(), n);
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

bool ReadAnimation(ByteReader& reader, AnimationDefinition& anim) {
    if (!reader.ReadString(anim.id)) {
        return false;
    }

    std::uint8_t kindByte = 0;
    if (!reader.ReadUint8(kindByte)) {
        return false;
    }
    if (kindByte > static_cast<std::uint8_t>(PlaybackKind::kOneShot)) {
        reader.Fail("animation '" + anim.id + "': invalid playback kind byte " + std::to_string(static_cast<int>(kindByte)));
        return false;
    }
    anim.kind = static_cast<PlaybackKind>(kindByte);

    if (!reader.ReadFloat64(anim.fps)) {
        return false;
    }

    std::uint8_t returnsByte = 0;
    if (!reader.ReadUint8(returnsByte)) {
        return false;
    }
    anim.returnsToIdle = returnsByte != 0;

    std::uint32_t frameCount = 0;
    if (!reader.ReadUint32(frameCount)) {
        return false;
    }

    anim.frames.clear();
    anim.frames.reserve(frameCount);
    int firstWidth = -1;
    int firstHeight = -1;

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        FrameDefinition frame;
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        double anchorX = 0.0;
        double anchorY = 0.0;
        double durationMs = 0.0;
        if (!reader.ReadUint32(w) || !reader.ReadUint32(h) || !reader.ReadFloat64(anchorX) ||
            !reader.ReadFloat64(anchorY) || !reader.ReadFloat64(durationMs)) {
            return false;
        }

        frame.width = static_cast<int>(w);
        frame.height = static_cast<int>(h);
        frame.anchor = core::Point{anchorX, anchorY};
        frame.durationMs = durationMs;

        if (firstWidth < 0) {
            firstWidth = frame.width;
            firstHeight = frame.height;
        } else if (frame.width != firstWidth || frame.height != firstHeight) {
            reader.Fail(
                "animation '" + anim.id + "': frame " + std::to_string(i) + " is " +
                std::to_string(frame.width) + "x" + std::to_string(frame.height) +
                ", expected " + std::to_string(firstWidth) + "x" + std::to_string(firstHeight) +
                " to match this animation's first frame");
            return false;
        }

        const std::size_t pixelBytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
        if (!reader.ReadPixels(frame.pixels, pixelBytes)) {
            return false;
        }

        anim.frames.push_back(std::move(frame));
    }

    if (anim.frames.empty()) {
        reader.Fail("animation '" + anim.id + "' has zero frames");
        return false;
    }

    return true;
}

}  // namespace

bool LoadPetPackFromMemory(const std::uint8_t* data, std::size_t size, PetDefinition& outPet, std::string& outError) {
    ByteReader reader(data, size);

    char magic[8];
    if (!reader.ReadBytes(magic, sizeof(magic))) {
        outError = reader.Error();
        return false;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        outError = "not a valid pet pack (bad magic)";
        return false;
    }

    PetDefinition pet;

    if (!reader.ReadString(pet.id) || !reader.ReadString(pet.displayName) || !reader.ReadString(pet.variantGroup)) {
        outError = reader.Error();
        return false;
    }

    std::uint32_t canvasWidth = 0;
    std::uint32_t canvasHeight = 0;
    if (!reader.ReadUint32(canvasWidth) || !reader.ReadUint32(canvasHeight)) {
        outError = reader.Error();
        return false;
    }
    pet.canvasWidth = static_cast<int>(canvasWidth);
    pet.canvasHeight = static_cast<int>(canvasHeight);

    if (!reader.ReadUint8(pet.alphaHitThreshold) || !reader.ReadFloat64(pet.passiveIntervalSeconds) ||
        !reader.ReadString(pet.contentVersion)) {
        outError = reader.Error();
        return false;
    }

    if (!ReadAnimation(reader, pet.idle)) {
        outError = reader.Error();
        return false;
    }
    if (!ReadAnimation(reader, pet.clickReaction)) {
        outError = reader.Error();
        return false;
    }

    std::uint32_t passiveCount = 0;
    if (!reader.ReadUint32(passiveCount)) {
        outError = reader.Error();
        return false;
    }
    pet.passiveActions.clear();
    pet.passiveActions.reserve(passiveCount);
    for (std::uint32_t i = 0; i < passiveCount; ++i) {
        AnimationDefinition anim;
        if (!ReadAnimation(reader, anim)) {
            outError = reader.Error();
            return false;
        }
        pet.passiveActions.push_back(std::move(anim));
    }

    // Sección final, opcional y aditiva (Block 04.2 — ver
    // docs/NIDIR_CONTENT.md): un pack compilado antes de este bloque
    // (p. ej. el bunny_pack.nvpack ya comiteado, nunca recompilado para
    // este bloque) simplemente termina acá, sin ningún byte más — eso
    // es válido y deja idleDirectionOverrides vacío, exactamente el
    // comportamiento no-direccional que ya tenía. Si SÍ quedan bytes,
    // se interpretan como esta sección — y, una vez que se sabe que
    // existe, cada campo adentro sigue siendo obligatorio (falla
    // ruidosamente igual que cualquier otro campo truncado/inválido).
    pet.idleDirectionOverrides.clear();
    if (reader.HasMoreData()) {
        std::uint32_t overrideCount = 0;
        if (!reader.ReadUint32(overrideCount)) {
            outError = reader.Error();
            return false;
        }
        pet.idleDirectionOverrides.reserve(overrideCount);
        for (std::uint32_t i = 0; i < overrideCount; ++i) {
            std::uint8_t directionByte = 0;
            if (!reader.ReadUint8(directionByte)) {
                outError = reader.Error();
                return false;
            }
            if (directionByte > static_cast<std::uint8_t>(Direction::kLeft)) {
                outError = "idleDirectionOverrides[" + std::to_string(i) + "]: invalid direction byte " + std::to_string(static_cast<int>(directionByte));
                return false;
            }

            DirectionalAnimationOverride override_;
            override_.direction = static_cast<Direction>(directionByte);
            if (!ReadAnimation(reader, override_.animation)) {
                outError = reader.Error();
                return false;
            }
            pet.idleDirectionOverrides.push_back(std::move(override_));
        }
    }

    if (pet.canvasWidth <= 0 || pet.canvasHeight <= 0) {
        outError = "invalid canvas size";
        return false;
    }
    if (pet.id.empty()) {
        outError = "pet id must not be empty";
        return false;
    }

    outPet = std::move(pet);
    return true;
}

bool LoadPetPackFromFile(const std::string& path, PetDefinition& outPet, std::string& outError) {
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

    return LoadPetPackFromMemory(buffer.data(), buffer.size(), outPet, outError);
}

}  // namespace nimvlets::content
