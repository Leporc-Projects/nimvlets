#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/AlphaMask.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::graphics {

// Loads a `.rgba` dev/QA fixture (the format tools/prep_dev_sprite.py
// produces from a source PNG) into memory, and can hand out both an
// SDL_Texture for rendering and a core::AlphaMask (derived from the
// image's *real* alpha channel, at an explicit threshold) for
// hit-testing.
//
// This is the Block 01 "Bunny" QA fixture loader (see
// docs/DECISION_LOG.md and docs/PLATFORM_SPIKE.md) — used to validate
// transparency/click-through hit-testing against a realistic,
// non-analytic asset before closing the block. It is deliberately NOT
// the content-loading system docs/PET_CONTENT_SPEC.md describes: no
// format negotiation, no atlas, no animation frames, no caching, no
// package/schema version, no provenance metadata. A future block's
// real content loader should not be assumed to look like this.
class DevSprite {
public:
    // Alpha (0-255) at/above which a source pixel counts as
    // "visible/interactive" when building the hit-test mask via
    // BuildAlphaMask(). 128 (50%) is the standard antialiased-edge
    // midpoint. Chosen after inspecting this specific fixture's alpha
    // histogram (background pixels are exactly 0; interior pixels
    // cluster at 253-254; only a thin edge band falls in between) —
    // see docs/DECISION_LOG.md for the full analysis.
    static constexpr std::uint8_t kHitTestAlphaThreshold = 128;

    // Loads `path` (tools/prep_dev_sprite.py's output format). Returns
    // false (and logs via SDL_Log) on any failure; the object is left
    // empty/unusable in that case — callers are expected to fall back
    // to the analytic placeholder (core::BlobSilhouette) rather than
    // crash, since this is a QA fixture, not a required asset.
    bool LoadFromFile(const std::string& path);

    int Width() const { return width_; }
    int Height() const { return height_; }
    bool IsLoaded() const { return !pixels_.empty(); }

    // Creates a new SDL_Texture from the loaded pixel data, with linear
    // scaling (this asset is native-resolution but drawn into a
    // possibly different-sized destination rect). Caller owns the
    // returned texture (SDL_DestroyTexture it). Returns nullptr on
    // failure or if nothing is loaded.
    SDL_Texture* CreateTexture(SDL_Renderer* renderer) const;

    // Rasterizes a core::AlphaMask at `targetWidth` x `targetHeight`
    // (nearest-neighbor sampled from the loaded native resolution) using
    // kHitTestAlphaThreshold. Pass the window's logical size here so the
    // resulting mask's coordinate space matches SDL mouse-event
    // coordinates directly, the same way core::BlobSilhouette::Contains()
    // already does — no separate scale factor needed at the call site.
    core::AlphaMask BuildAlphaMask(int targetWidth, int targetHeight) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;  // RGBA8, row-major, top-to-bottom
};

}  // namespace nimvlets::graphics
