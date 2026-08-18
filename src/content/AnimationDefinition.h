#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Geometry.h"

namespace nimvlets::content {

// How an animation's frames advance over time. See
// docs/ANIMATION_RUNTIME.md for the full behavioral contract of each.
enum class PlaybackKind : std::uint8_t {
    kStatic = 0,   // exactly one frame, never advances — no frame deadline ever exists.
    kLoop = 1,     // wraps back to frame 0 after the last frame.
    kOneShot = 2,  // plays once, then the controller returns to Idle (see AnimationController).
};

// One displayable frame: pixel data plus enough metadata to place it
// consistently and time it correctly. Deliberately pure C++ (no SDL) —
// `pixels` is a plain RGBA8 buffer content::PetPackLoader fills in from
// a compiled pack; the graphics layer turns it into a texture and never
// mutates it. `rendererHandle` is the one deliberate exception: an
// opaque slot the graphics layer may attach a native texture handle to
// after loading, so per-frame textures aren't recreated every time the
// same frame is shown again. content:: never reads or interprets it.
struct FrameDefinition {
    int width = 0;
    int height = 0;

    // The point (in this frame's own pixel coordinates) that should
    // align to the pet's canvas center when rendered — keeps frames
    // with different internal content placement (e.g. a squash/stretch
    // transform) from visually jittering relative to each other, even
    // though every frame in this block's dev content happens to share
    // one fixed canvas size. See docs/ANIMATION_RUNTIME.md.
    core::Point anchor{};

    // Milliseconds this frame stays on screen when its
    // AnimationDefinition uses per-frame durations (AnimationDefinition
    // ::fps == 0). Ignored when fps > 0.
    double durationMs = 0.0;

    // RGBA8, row-major, top-to-bottom, straight alpha.
    // Size must be exactly width * height * 4 bytes.
    std::vector<std::uint8_t> pixels;

    // Opaque; owned/interpreted only by the graphics layer (an
    // SDL_Texture*, once attached). nullptr until the graphics layer
    // attaches one.
    void* rendererHandle = nullptr;
};

// An ordered sequence of frames plus how they play back. Stable `id` is
// data, not an enum — new animations never require new C++.
struct AnimationDefinition {
    std::string id;
    PlaybackKind kind = PlaybackKind::kStatic;

    // > 0: every frame shows for 1000/fps ms, frames[i].durationMs is
    //      ignored.
    // == 0: each frame shows for its own frames[i].durationMs.
    double fps = 0.0;

    // Whether AnimationController transitions back to Idle when this
    // (one-shot) animation completes. Meaningless for kStatic/kLoop.
    bool returnsToIdle = true;

    std::vector<FrameDefinition> frames;

    // How long `frameIndex` stays on screen, in ms. Returns 0.0 for an
    // out-of-range index or a malformed (fps<=0 and durationMs<=0)
    // frame — callers treat 0.0 as "cannot advance" (effectively
    // static), never as "advance instantly forever".
    double FrameDurationMs(std::size_t frameIndex) const;
};

// Everything needed to run one kind of Nimvlet: its idle look, its
// click reaction, its passive actions, and the thresholds/timing that
// govern them. One logical Nimvlet — `variantGroup` exists so a future
// block can express "Frin has male/female variants" in data, without
// implementing selection/unlocking here (see docs/DECISION_LOG.md).
struct PetDefinition {
    std::string id;
    std::string displayName;

    // Empty string = no variant grouping. Non-empty = this pet is one
    // variant among others sharing the same group id (e.g. "frin").
    // Not used for anything yet in this block — schema-only, per the
    // block brief.
    std::string variantGroup;

    int canvasWidth = 160;
    int canvasHeight = 160;

    // alpha >= this value counts as visible/interactive (see
    // core::AlphaMask::FromAlphaChannel). Configurable per pet, not a
    // global constant — docs/ANIMATION_RUNTIME.md documents why 128 is
    // the default.
    std::uint8_t alphaHitThreshold = 128;

    AnimationDefinition idle;
    AnimationDefinition clickReaction;

    // Zero or more sparse autonomous actions; AnimationController picks
    // which one to play by index (see TriggerPassiveAction()).
    std::vector<AnimationDefinition> passiveActions;

    // Target average seconds between passive actions. A scheduling
    // target, not a hard guarantee — see docs/ANIMATION_RUNTIME.md.
    double passiveIntervalSeconds = 300.0;

    // Optional; empty string if not set. Schema-only in this block —
    // nothing reads it yet, but the field exists so future content
    // packs have a place to record it without a schema change.
    std::string contentVersion;
};

}  // namespace nimvlets::content
