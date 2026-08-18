#include "ClickAccountingTest.h"

#include "content/AnimationController.h"
#include "core/DragClassifier.h"

// Integration test: exercises core::DragClassifier and
// content::AnimationController wired together exactly the way
// src/app/SpikeApp does (see its MOUSE_BUTTON_UP handling) — click
// *counting* is unconditional on a valid click and entirely independent
// of whatever the animation controller does with that same click (see
// block brief §2B and docs/ANIMATION_RUNTIME.md). Both types are
// already pure/SDL-free, so this needs no mocking at all.

using nimvlets::content::AnimationController;
using nimvlets::content::AnimationDefinition;
using nimvlets::content::FrameDefinition;
using nimvlets::content::PetDefinition;
using nimvlets::content::PlaybackKind;
using nimvlets::core::DragClassifier;
using nimvlets::core::Point;
using nimvlets::core::PointerGesture;

namespace nimvlets::tests {

namespace {

FrameDefinition MakeFrame(double durationMs) {
    FrameDefinition frame;
    frame.width = 4;
    frame.height = 4;
    frame.durationMs = durationMs;
    return frame;
}

PetDefinition MakeTestPet() {
    PetDefinition pet;
    pet.id = "test_pet";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;

    pet.idle.id = "idle";
    pet.idle.kind = PlaybackKind::kStatic;
    pet.idle.frames = {MakeFrame(0.0)};

    pet.clickReaction.id = "click_reaction";
    pet.clickReaction.kind = PlaybackKind::kOneShot;
    pet.clickReaction.returnsToIdle = true;
    pet.clickReaction.frames = {MakeFrame(200.0), MakeFrame(200.0), MakeFrame(200.0)};

    return pet;
}

// Mirrors SpikeApp's SDL_EVENT_MOUSE_BUTTON_UP handling: ends the
// gesture, and only on a valid click both increments the counter *and*
// tells the animation controller — a drag does neither.
void EndGesture(DragClassifier& drag, AnimationController& anim, Point end, double nowMs, int& clickCount) {
    const PointerGesture gesture = drag.End(end);
    if (gesture == PointerGesture::kClick) {
        ++clickCount;
        anim.TriggerClick(nowMs);
    }
}

bool ValidClickIncrementsCounterAndTriggersReaction() {
    PetDefinition pet = MakeTestPet();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    drag.Begin(Point{10.0, 10.0});
    EndGesture(drag, anim, Point{10.0, 10.0}, 0.0, clickCount);

    NIMVLETS_CHECK(clickCount == 1);
    NIMVLETS_CHECK(anim.State() == nimvlets::content::ControllerState::kClickReaction);
    return true;
}

bool DragDoesNotIncrementCounterOrTriggerReaction() {
    PetDefinition pet = MakeTestPet();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    drag.Begin(Point{10.0, 10.0});
    drag.Update(Point{50.0, 10.0});  // well beyond the default click/drag threshold
    EndGesture(drag, anim, Point{50.0, 10.0}, 0.0, clickCount);

    NIMVLETS_CHECK(clickCount == 0);
    NIMVLETS_CHECK(anim.State() == nimvlets::content::ControllerState::kIdle);
    return true;
}

bool MultipleValidClicksCountWhileVisualReactionStaysCoalesced() {
    PetDefinition pet = MakeTestPet();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    // First click: starts the reaction at frame 0.
    drag.Begin(Point{1.0, 1.0});
    EndGesture(drag, anim, Point{1.0, 1.0}, 0.0, clickCount);
    NIMVLETS_CHECK(clickCount == 1);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &pet.clickReaction.frames[0]);

    // Nudge the animation partway (not far enough to change frame:
    // 200ms per frame, only 50ms elapsed).
    anim.Advance(50.0);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &pet.clickReaction.frames[0]);

    // A second valid click while the reaction is still playing: counts,
    // but must NOT restart the visual (still frame 0, not reset).
    drag.Begin(Point{2.0, 2.0});
    EndGesture(drag, anim, Point{2.0, 2.0}, 60.0, clickCount);
    NIMVLETS_CHECK(clickCount == 2);
    NIMVLETS_CHECK(anim.State() == nimvlets::content::ControllerState::kClickReaction);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &pet.clickReaction.frames[0]);

    // A third: same story. Rapid autoclicking must never "thrash" the
    // visual reaction, only the counter.
    drag.Begin(Point{3.0, 3.0});
    EndGesture(drag, anim, Point{3.0, 3.0}, 70.0, clickCount);
    NIMVLETS_CHECK(clickCount == 3);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &pet.clickReaction.frames[0]);

    return true;
}

bool DragBetweenTwoValidClicksNeitherCountsNorRestarts() {
    PetDefinition pet = MakeTestPet();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    drag.Begin(Point{0.0, 0.0});
    EndGesture(drag, anim, Point{0.0, 0.0}, 0.0, clickCount);
    NIMVLETS_CHECK(clickCount == 1);

    drag.Begin(Point{0.0, 0.0});
    drag.Update(Point{100.0, 0.0});
    EndGesture(drag, anim, Point{100.0, 0.0}, 10.0, clickCount);
    NIMVLETS_CHECK(clickCount == 1);  // unchanged by the drag

    drag.Begin(Point{0.0, 0.0});
    EndGesture(drag, anim, Point{0.0, 0.0}, 20.0, clickCount);
    NIMVLETS_CHECK(clickCount == 2);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &pet.clickReaction.frames[0]);  // still coalesced

    return true;
}

}  // namespace

void RegisterClickAccountingTests(testing::TestRunner& runner) {
    runner.Add("ClickAccounting/ValidClickIncrementsCounterAndTriggersReaction", ValidClickIncrementsCounterAndTriggersReaction);
    runner.Add("ClickAccounting/DragDoesNotIncrementCounterOrTriggerReaction", DragDoesNotIncrementCounterOrTriggerReaction);
    runner.Add(
        "ClickAccounting/MultipleValidClicksCountWhileVisualReactionStaysCoalesced",
        MultipleValidClicksCountWhileVisualReactionStaysCoalesced);
    runner.Add("ClickAccounting/DragBetweenTwoValidClicksNeitherCountsNorRestarts", DragBetweenTwoValidClicksNeitherCountsNorRestarts);
}

}  // namespace nimvlets::tests
