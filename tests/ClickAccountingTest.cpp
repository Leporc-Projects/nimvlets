#include "ClickAccountingTest.h"

#include "TestPetFixtures.h"
#include "content/AnimationController.h"
#include "core/DragClassifier.h"

// Integration test: exercises core::DragClassifier and
// content::AnimationController wired together exactly the way
// src/app/SpikeApp does (see its MOUSE_BUTTON_UP handling) — click
// *counting* is unconditional on a valid click and entirely independent
// of whatever the animation controller does with that same click. Both
// types are already pure/SDL-free, so this needs no mocking at all.

using nimvlets::content::AnimationController;
using nimvlets::content::ControllerMode;
using nimvlets::content::PetDefinition;
using nimvlets::core::DragClassifier;
using nimvlets::core::Point;
using nimvlets::core::PointerGesture;

namespace nimvlets::tests {

namespace {

// Mirrors SpikeApp's SDL_EVENT_MOUSE_BUTTON_UP handling: ends the
// gesture, and only on a valid click both increments the counter *and*
// tells the animation controller — a drag does neither.
void EndGesture(DragClassifier& drag, AnimationController& anim, Point end, double nowMs, int& clickCount) {
    const PointerGesture gesture = drag.End(end);
    if (gesture == PointerGesture::kClick) {
        ++clickCount;
        anim.TriggerClick(0.0, nowMs);
    }
}

bool ValidClickIncrementsCounterAndTriggersReaction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    drag.Begin(Point{10.0, 10.0});
    EndGesture(drag, anim, Point{10.0, 10.0}, 0.0, clickCount);

    NIMVLETS_CHECK(clickCount == 1);
    NIMVLETS_CHECK(anim.Mode() == ControllerMode::kClickAction);
    return true;
}

bool DragDoesNotIncrementCounterOrTriggerReaction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;

    drag.Begin(Point{10.0, 10.0});
    drag.Update(Point{50.0, 10.0});  // well beyond the default click/drag threshold
    EndGesture(drag, anim, Point{50.0, 10.0}, 0.0, clickCount);

    NIMVLETS_CHECK(clickCount == 0);
    NIMVLETS_CHECK(anim.Mode() == ControllerMode::kBase);
    return true;
}

bool MultipleValidClicksCountWhileVisualReactionStaysCoalesced() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;
    const auto& clickFrames = pet.states[0].clickActions[0].animation.frames;

    // First click: starts the reaction at frame 0.
    drag.Begin(Point{1.0, 1.0});
    EndGesture(drag, anim, Point{1.0, 1.0}, 0.0, clickCount);
    NIMVLETS_CHECK(clickCount == 1);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &clickFrames[0]);

    // Nudge the animation partway (not far enough to change frame:
    // 100ms per frame, only 50ms elapsed).
    anim.Advance(50.0);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &clickFrames[0]);

    // A second valid click while the reaction is still playing: counts,
    // but must NOT restart the visual (still frame 0, not reset).
    drag.Begin(Point{2.0, 2.0});
    EndGesture(drag, anim, Point{2.0, 2.0}, 60.0, clickCount);
    NIMVLETS_CHECK(clickCount == 2);
    NIMVLETS_CHECK(anim.Mode() == ControllerMode::kClickAction);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &clickFrames[0]);

    // A third: same story. Rapid autoclicking must never "thrash" the
    // visual reaction, only the counter.
    drag.Begin(Point{3.0, 3.0});
    EndGesture(drag, anim, Point{3.0, 3.0}, 70.0, clickCount);
    NIMVLETS_CHECK(clickCount == 3);
    NIMVLETS_CHECK(&anim.CurrentFrame() == &clickFrames[0]);

    return true;
}

bool DragBetweenTwoValidClicksNeitherCountsNorRestarts() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController anim(pet);
    DragClassifier drag;
    int clickCount = 0;
    const auto& clickFrames = pet.states[0].clickActions[0].animation.frames;

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
    NIMVLETS_CHECK(&anim.CurrentFrame() == &clickFrames[0]);  // still coalesced

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
