#include "content/AnimationController.h"

namespace nimvlets::content {

AnimationController::AnimationController(const PetDefinition& pet)
    : pet_(pet), currentAnimation_(&pet_.idle) {}

bool AnimationController::Advance(double nowMs) {
    bool changed = false;

    // Loop rather than step once: if the caller was asleep long enough
    // to cross more than one frame boundary (or finish a whole one-shot
    // animation), catch all the way up to `nowMs` in one call instead of
    // needing to be called once per elapsed frame. Guaranteed to
    // terminate because currentFrameStartMs_ strictly increases by a
    // positive duration each iteration, or the loop returns.
    while (true) {
        if (currentAnimation_->kind == PlaybackKind::kStatic) {
            return changed;
        }

        const double duration = currentAnimation_->FrameDurationMs(currentFrameIndex_);
        if (duration <= 0.0) {
            // Malformed (or empty) animation — nothing sensible to
            // advance to. Treat like static rather than spin.
            return changed;
        }
        if (nowMs < currentFrameStartMs_ + duration) {
            return changed;
        }

        currentFrameStartMs_ += duration;
        changed = true;
        ++currentFrameIndex_;

        if (currentFrameIndex_ < currentAnimation_->frames.size()) {
            continue;  // still within this animation; check the new frame's deadline too
        }

        // Ran off the end of the animation.
        if (currentAnimation_->kind == PlaybackKind::kLoop) {
            currentFrameIndex_ = 0;
            continue;
        }

        // One-shot finished.
        if (currentAnimation_->returnsToIdle) {
            TransitionToIdle(currentFrameStartMs_);
        } else {
            // Hold on the last frame rather than reading out of bounds.
            currentFrameIndex_ = currentAnimation_->frames.size() - 1;
            return changed;
        }
    }
}

void AnimationController::TriggerClick(double nowMs) {
    if (state_ == ControllerState::kClickReaction) {
        return;  // already playing — coalesce, do not restart (block brief §2B/§6)
    }
    state_ = ControllerState::kClickReaction;
    currentAnimation_ = &pet_.clickReaction;
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
}

void AnimationController::TriggerPassiveAction(std::size_t passiveActionIndex, double nowMs) {
    if (state_ != ControllerState::kIdle) {
        return;  // lower priority than click reaction and than an already-running passive action
    }
    if (passiveActionIndex >= pet_.passiveActions.size()) {
        return;
    }
    state_ = ControllerState::kPassiveAction;
    currentAnimation_ = &pet_.passiveActions[passiveActionIndex];
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
}

const FrameDefinition& AnimationController::CurrentFrame() const {
    return currentAnimation_->frames[currentFrameIndex_];
}

std::optional<double> AnimationController::NextFrameDeadlineMs() const {
    if (currentAnimation_->kind == PlaybackKind::kStatic) {
        return std::nullopt;
    }
    const double duration = currentAnimation_->FrameDurationMs(currentFrameIndex_);
    if (duration <= 0.0) {
        return std::nullopt;
    }
    return currentFrameStartMs_ + duration;
}

void AnimationController::TransitionToIdle(double nowMs) {
    state_ = ControllerState::kIdle;
    currentAnimation_ = &pet_.idle;
    currentFrameIndex_ = 0;
    currentFrameStartMs_ = nowMs;
}

}  // namespace nimvlets::content
