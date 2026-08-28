#include "PreviewArtifactTest.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "productui/PreviewArtifact.h"

using nimvlets::productui::LoadPetPreviewFromMemory;
using nimvlets::productui::PetPreviewImage;
using nimvlets::productui::PreviewPathForPack;

namespace nimvlets::tests {

namespace {

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void AppendString(std::vector<std::uint8_t>& out, const std::string& s) {
    AppendU32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

// Construye un NVPREV1 válido de `w`x`h` con un patrón RGBA reconocible
// (cada byte = su índice mod 251, así el alpha varía de verdad).
std::vector<std::uint8_t> BuildValidPreview(
    const std::string& petId, const std::string& variantId, int w, int h) {
    std::vector<std::uint8_t> out;
    const char magic[8] = {'N', 'V', 'P', 'R', 'E', 'V', '1', '\0'};
    out.insert(out.end(), magic, magic + 8);
    AppendU32(out, 1);  // version
    AppendString(out, petId);
    AppendString(out, variantId);
    AppendString(out, "some_pack.nvpack");
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(w) * static_cast<std::uint32_t>(h) * 4;
    AppendU32(out, static_cast<std::uint32_t>(w));
    AppendU32(out, static_cast<std::uint32_t>(h));
    AppendU32(out, pixelBytes);
    for (std::uint32_t i = 0; i < pixelBytes; ++i) {
        out.push_back(static_cast<std::uint8_t>(i % 251));
    }
    return out;
}

bool TestRoundTripPreservesDimensionsAndPixels() {
    const std::vector<std::uint8_t> buf = BuildValidPreview("frin", "female", 5, 3);
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(img.petId == "frin");
    NIMVLETS_CHECK(img.variantId == "female");
    NIMVLETS_CHECK(img.sourcePack == "some_pack.nvpack");
    NIMVLETS_CHECK(img.width == 5 && img.height == 3);
    NIMVLETS_CHECK(img.rgba.size() == 5u * 3u * 4u);
    // Alpha preservado exactamente byte a byte (straight alpha, sin
    // premultiplicar ni tocar) — el pixel 0 empieza en 0,1,2,3.
    NIMVLETS_CHECK(img.rgba[3] == 3);
    NIMVLETS_CHECK(img.rgba[7] == 7);
    NIMVLETS_CHECK(img.rgba[59] == static_cast<std::uint8_t>(59 % 251));
    return true;
}

bool TestNoVariantPreviewLoads() {
    const std::vector<std::uint8_t> buf = BuildValidPreview("bunny", "", 4, 4);
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(img.petId == "bunny");
    NIMVLETS_CHECK(img.variantId.empty());
    return true;
}

bool TestRejectsBadMagic() {
    std::vector<std::uint8_t> buf = BuildValidPreview("frin", "male", 4, 4);
    buf[1] = 'X';
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(err.find("magic") != std::string::npos);
    return true;
}

bool TestRejectsUnsupportedVersion() {
    std::vector<std::uint8_t> buf = BuildValidPreview("frin", "male", 4, 4);
    buf[8] = 9;  // version byte (LE) -> 9
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(err.find("version") != std::string::npos);
    return true;
}

bool TestRejectsTruncatedPixels() {
    std::vector<std::uint8_t> buf = BuildValidPreview("frin", "male", 8, 8);
    buf.resize(buf.size() - 10);  // corta 10 bytes de píxeles
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(err.find("truncat") != std::string::npos);
    return true;
}

bool TestRejectsPixelBytesMismatch() {
    const std::string petId = "frin";
    const std::string variantId = "male";
    std::vector<std::uint8_t> buf = BuildValidPreview(petId, variantId, 4, 4);
    // Offset del campo pixel_bytes: magic(8) + version(4) + 3 strings
    // (len+bytes) + width(4) + height(4).
    const std::size_t off = 8 + 4 + (4 + petId.size()) + (4 + variantId.size()) +
                            (4 + std::string("some_pack.nvpack").size()) + 4 + 4;
    buf[off] = 63;  // 63 != 4*4*4 = 64
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    NIMVLETS_CHECK(err.find("pixel_bytes") != std::string::npos);
    return true;
}

bool TestRejectsZeroDimension() {
    const std::vector<std::uint8_t> buf = BuildValidPreview("frin", "male", 0, 4);
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(buf.data(), buf.size(), img, err));
    return true;
}

bool TestRejectsEmptyBuffer() {
    PetPreviewImage img;
    std::string err;
    NIMVLETS_CHECK(!LoadPetPreviewFromMemory(nullptr, 0, img, err));
    return true;
}

bool TestPreviewPathConvention() {
    NIMVLETS_CHECK(PreviewPathForPack("assets/dev/frin_male_pack.nvpack") == "assets/dev/frin_male_pack.nvprev");
    NIMVLETS_CHECK(PreviewPathForPack("bunny_pack.nvpack") == "bunny_pack.nvprev");
    // Sin sufijo .nvpack -> se agrega .nvprev (defensivo).
    NIMVLETS_CHECK(PreviewPathForPack("weird_name") == "weird_name.nvprev");
    NIMVLETS_CHECK(PreviewPathForPack("") == ".nvprev");
    return true;
}

}  // namespace

void RegisterPreviewArtifactTests(testing::TestRunner& runner) {
    runner.Add("PreviewArtifact/RoundTripPreservesDimensionsAndPixels", TestRoundTripPreservesDimensionsAndPixels);
    runner.Add("PreviewArtifact/NoVariantPreviewLoads", TestNoVariantPreviewLoads);
    runner.Add("PreviewArtifact/RejectsBadMagic", TestRejectsBadMagic);
    runner.Add("PreviewArtifact/RejectsUnsupportedVersion", TestRejectsUnsupportedVersion);
    runner.Add("PreviewArtifact/RejectsTruncatedPixels", TestRejectsTruncatedPixels);
    runner.Add("PreviewArtifact/RejectsPixelBytesMismatch", TestRejectsPixelBytesMismatch);
    runner.Add("PreviewArtifact/RejectsZeroDimension", TestRejectsZeroDimension);
    runner.Add("PreviewArtifact/RejectsEmptyBuffer", TestRejectsEmptyBuffer);
    runner.Add("PreviewArtifact/PreviewPathConvention", TestPreviewPathConvention);
}

}  // namespace nimvlets::tests
