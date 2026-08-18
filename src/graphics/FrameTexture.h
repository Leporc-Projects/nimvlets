#pragma once

#include "content/AnimationDefinition.h"

struct SDL_Renderer;

namespace nimvlets::graphics {

// The rendering-layer half of content::FrameDefinition::rendererHandle's
// contract (see that field's doc comment): turns one frame's raw RGBA8
// pixel data into an SDL_Texture and attaches it to the frame so it is
// created exactly once per frame, not re-uploaded every time the same
// frame is shown again (idle repeats, one-shots replay, ...).
//
// This is Block 02's replacement for Block 01's graphics::DevSprite,
// generalized from "one hardcoded QA fixture" to "any frame of any
// data-driven content::PetDefinition" — see docs/ANIMATION_RUNTIME.md.

// Creates an SDL_Texture from `frame`'s pixel data and stores it in
// `frame.rendererHandle`. A no-op (returns true) if a texture is already
// attached. Returns false and logs via SDL_Log on failure (malformed
// frame data, or an SDL texture-creation failure), leaving
// rendererHandle null.
bool AttachFrameTexture(SDL_Renderer* renderer, content::FrameDefinition& frame);

// Destroys the SDL_Texture attached to `frame.rendererHandle`, if any,
// and resets the handle to nullptr. Safe to call on a frame with no
// attached texture.
void ReleaseFrameTexture(content::FrameDefinition& frame);

}  // namespace nimvlets::graphics
