#include "AnimationControllerTest.h"

#include "content/AnimationController.h"

using nimvlets::content::AnimationDefinition;
using nimvlets::content::AnimationController;
using nimvlets::content::ControllerState;
using nimvlets::content::FrameDefinition;
using nimvlets::content::PetDefinition;
using nimvlets::content::PlaybackKind;

namespace nimvlets::tests {

namespace {

FrameDefinition MakeFrame(double durationMs) {
    FrameDefinition frame;
    frame.width = 4;
    frame.height = 4;
    frame.durationMs = durationMs;
    return frame;
}

// idle: static, 1 frame.
// clickReaction: one-shot, 3 frames @ 100ms each (300ms total).
// passiveActions[0] "wiggle": one-shot, 2 frames @ 50ms each (100ms total).
PetDefinition MakeTestPet() {
    PetDefinition pet;
    pet.id = "test_pet";
    pet.displayName = "Test Pet";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;

    pet.idle.id = "idle";
    pet.idle.kind = PlaybackKind::kStatic;
    pet.idle.frames = {MakeFrame(0.0)};

    pet.clickReaction.id = "click_reaction";
    pet.clickReaction.kind = PlaybackKind::kOneShot;
    pet.clickReaction.returnsToIdle = true;
    pet.clickReaction.frames = {MakeFrame(100.0), MakeFrame(100.0), MakeFrame(100.0)};

    AnimationDefinition wiggle;
    wiggle.id = "passive_wiggle";
    wiggle.kind = PlaybackKind::kOneShot;
    wiggle.returnsToIdle = true;
    wiggle.frames = {MakeFrame(50.0), MakeFrame(50.0)};
    pet.passiveActions = {wiggle};

    return pet;
}

bool StaticIdleHasNoFutureFrameDeadline() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.State() == ControllerState::kIdle);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    // Even a huge time jump changes nothing for a static animation.
    NIMVLETS_CHECK(!controller.Advance(1'000'000.0));
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

bool OneShotProgressesCorrectly() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    NIMVLETS_CHECK(controller.State() == ControllerState::kClickReaction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[0]);

    NIMVLETS_CHECK(!controller.Advance(50.0));  // before frame 0's deadline
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[0]);

    NIMVLETS_CHECK(controller.Advance(100.0));  // frame 0 -> 1
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[1]);

    NIMVLETS_CHECK(controller.Advance(200.0));  // frame 1 -> 2
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[2]);
    return true;
}

bool OneShotReturnsToIdle() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    NIMVLETS_CHECK(controller.Advance(300.0));  // 3 frames @ 100ms => finishes exactly at 300ms
    NIMVLETS_CHECK(controller.State() == ControllerState::kIdle);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.idle.frames[0]);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

bool FrameBoundaryJustBeforeDoesNotAdvance() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    NIMVLETS_CHECK(!controller.Advance(99.999));
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[0]);
    return true;
}

bool FrameBoundaryExactlyAtDeadlineAdvances() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    NIMVLETS_CHECK(controller.Advance(100.0));
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[1]);
    return true;
}

bool ClickReactionInterruptsPassiveAction() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerPassiveAction(0, 0.0);
    NIMVLETS_CHECK(controller.State() == ControllerState::kPassiveAction);
    controller.Advance(25.0);  // partway through the passive action, still frame 0

    controller.TriggerClick(30.0);
    NIMVLETS_CHECK(controller.State() == ControllerState::kClickReaction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[0]);
    return true;
}

bool RepeatedClickDuringReactionDoesNotRestartVisual() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    controller.Advance(100.0);  // now on frame 1
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[1]);

    controller.TriggerClick(150.0);  // a second, "repeated" click mid-reaction
    NIMVLETS_CHECK(controller.State() == ControllerState::kClickReaction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[1]);  // unchanged, not reset to 0
    return true;
}

bool PassiveActionNeverInterruptsClickReaction() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0);
    controller.TriggerPassiveAction(0, 10.0);  // lower priority — must be ignored
    NIMVLETS_CHECK(controller.State() == ControllerState::kClickReaction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.clickReaction.frames[0]);
    return true;
}

bool PassiveActionPlaysAndReturnsToIdle() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerPassiveAction(0, 0.0);
    NIMVLETS_CHECK(controller.State() == ControllerState::kPassiveAction);
    NIMVLETS_CHECK(controller.Advance(100.0));  // 2 frames @ 50ms => finishes exactly at 100ms
    NIMVLETS_CHECK(controller.State() == ControllerState::kIdle);
    return true;
}

bool OutOfRangePassiveIndexIsIgnored() {
    PetDefinition pet = MakeTestPet();
    AnimationController controller(pet);
    controller.TriggerPassiveAction(99, 0.0);
    NIMVLETS_CHECK(controller.State() == ControllerState::kIdle);
    return true;
}

bool LoopAnimationWrapsAndCanCatchUpMultipleFrameBoundariesInOneCall() {
    PetDefinition pet = MakeTestPet();
    pet.idle.kind = PlaybackKind::kLoop;
    pet.idle.frames = {MakeFrame(10.0), MakeFrame(10.0), MakeFrame(10.0)};  // 3 frames, 30ms full cycle
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.NextFrameDeadlineMs().has_value());  // a loop always has a pending deadline

    // One full lap (30ms) plus one more frame (10ms) = frame index 1, all
    // caught up within a single Advance() call.
    NIMVLETS_CHECK(controller.Advance(40.0));
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.idle.frames[1]);
    return true;
}

// --- ChooseWeightedPassiveActionIndex (Block 04.3, corrección post-QA
// -- política 70/30) ---

PetDefinition MakeTwoPassiveActionPet(double weight0, double weight1) {
    PetDefinition pet = MakeTestPet();
    AnimationDefinition second;
    second.id = "passive_second";
    second.kind = PlaybackKind::kOneShot;
    second.returnsToIdle = true;
    second.frames = {MakeFrame(50.0)};
    pet.passiveActions.push_back(second);  // ahora 2 entradas: [0]=wiggle, [1]=second
    pet.passiveActionWeights = {weight0, weight1};
    return pet;
}

bool WeightedSelectionRespects70_30SplitAtBoundaries() {
    PetDefinition pet = MakeTwoPassiveActionPet(0.7, 0.3);
    // target = uniformRandom01 * totalWeight(=1.0); index 0 mientras
    // target < 0.7, index 1 en cuanto target >= 0.7.
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.0) == 0);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.69) == 0);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.70) == 1);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.99) == 1);
    return true;
}

bool WeightedSelectionIsDeterministicGivenSameInput() {
    PetDefinition pet = MakeTwoPassiveActionPet(0.7, 0.3);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.42) == ChooseWeightedPassiveActionIndex(pet, 0.42));
    return true;
}

bool WeightedSelectionFallsBackToUniformWithoutExplicitWeights() {
    PetDefinition pet = MakeTestPet();  // 1 sola acción pasiva, sin pesos
    NIMVLETS_CHECK(pet.passiveActionWeights.empty());
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.0) == 0);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.99) == 0);
    return true;
}

bool WeightedSelectionFallsBackToUniformOnSizeMismatch() {
    PetDefinition pet = MakeTwoPassiveActionPet(0.7, 0.3);
    pet.passiveActionWeights = {1.0};  // tamaño inconsistente con 2 passiveActions -- defensivo
    // Selección uniforme sobre 2 entradas: [0, 0.5) -> 0, [0.5, 1) -> 1.
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.0) == 0);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 0.6) == 1);
    return true;
}

bool WeightedSelectionClampsOutOfRangeRandomInput() {
    PetDefinition pet = MakeTwoPassiveActionPet(0.7, 0.3);
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, -1.0) == 0);   // clamp a 0.0 -> index 0
    NIMVLETS_CHECK(ChooseWeightedPassiveActionIndex(pet, 5.0) == 1);    // clamp a <1.0 -> index 1
    return true;
}

}  // namespace

void RegisterAnimationControllerTests(testing::TestRunner& runner) {
    runner.Add("AnimationController/StaticIdleHasNoFutureFrameDeadline", StaticIdleHasNoFutureFrameDeadline);
    runner.Add("AnimationController/OneShotProgressesCorrectly", OneShotProgressesCorrectly);
    runner.Add("AnimationController/OneShotReturnsToIdle", OneShotReturnsToIdle);
    runner.Add("AnimationController/FrameBoundaryJustBeforeDoesNotAdvance", FrameBoundaryJustBeforeDoesNotAdvance);
    runner.Add("AnimationController/FrameBoundaryExactlyAtDeadlineAdvances", FrameBoundaryExactlyAtDeadlineAdvances);
    runner.Add("AnimationController/ClickReactionInterruptsPassiveAction", ClickReactionInterruptsPassiveAction);
    runner.Add("AnimationController/RepeatedClickDuringReactionDoesNotRestartVisual", RepeatedClickDuringReactionDoesNotRestartVisual);
    runner.Add("AnimationController/PassiveActionNeverInterruptsClickReaction", PassiveActionNeverInterruptsClickReaction);
    runner.Add("AnimationController/PassiveActionPlaysAndReturnsToIdle", PassiveActionPlaysAndReturnsToIdle);
    runner.Add("AnimationController/OutOfRangePassiveIndexIsIgnored", OutOfRangePassiveIndexIsIgnored);
    runner.Add(
        "AnimationController/LoopAnimationWrapsAndCanCatchUpMultipleFrameBoundariesInOneCall",
        LoopAnimationWrapsAndCanCatchUpMultipleFrameBoundariesInOneCall);
    runner.Add("AnimationController/WeightedSelectionRespects70_30SplitAtBoundaries", WeightedSelectionRespects70_30SplitAtBoundaries);
    runner.Add("AnimationController/WeightedSelectionIsDeterministicGivenSameInput", WeightedSelectionIsDeterministicGivenSameInput);
    runner.Add(
        "AnimationController/WeightedSelectionFallsBackToUniformWithoutExplicitWeights",
        WeightedSelectionFallsBackToUniformWithoutExplicitWeights);
    runner.Add(
        "AnimationController/WeightedSelectionFallsBackToUniformOnSizeMismatch",
        WeightedSelectionFallsBackToUniformOnSizeMismatch);
    runner.Add("AnimationController/WeightedSelectionClampsOutOfRangeRandomInput", WeightedSelectionClampsOutOfRangeRandomInput);
}

}  // namespace nimvlets::tests
