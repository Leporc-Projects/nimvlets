#include "AnimationControllerTest.h"

#include "TestPetFixtures.h"
#include "content/AnimationController.h"

using nimvlets::content::AnimationController;
using nimvlets::content::ChooseWeightedActionIndex;
using nimvlets::content::ControllerMode;
using nimvlets::content::Direction;
using nimvlets::content::PetDefinition;
using nimvlets::content::WeightedAction;

namespace nimvlets::tests {

namespace {

bool StaticBaseHasNoFutureFrameDeadline() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    // Even a huge time jump changes nothing for a static base animation.
    NIMVLETS_CHECK(!controller.Advance(1'000'000.0));
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

bool OneShotProgressesCorrectly() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerClick(0.0, 0.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    const auto& clickAnim = pet.states[0].clickActions[0].animation;
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[0]);

    NIMVLETS_CHECK(!controller.Advance(50.0));  // before frame 0's deadline
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[0]);

    NIMVLETS_CHECK(controller.Advance(100.0));  // frame 0 -> 1
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[1]);

    NIMVLETS_CHECK(controller.Advance(200.0));  // frame 1 -> 2
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[2]);
    return true;
}

bool ClickCompletesAndReturnsToBase() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(controller.Advance(300.0));  // exactly the total duration -> finishes
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(controller.CurrentStateId() == "default");
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    return true;
}

bool RepeatedClickCoalescesInsteadOfRestarting() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerClick(0.0, 0.0));
    controller.Advance(100.0);  // now on frame 1
    NIMVLETS_CHECK(!controller.TriggerClick(0.0, 150.0));  // coalesce -- no-op
    const auto& clickAnim = pet.states[0].clickActions[0].animation;
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[1]);  // still frame 1, not restarted
    return true;
}

bool ClickInterruptsAmbientAction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerAmbientAction(0.0, 0.0));  // picks weight-0.7 "breathing"
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);

    NIMVLETS_CHECK(controller.TriggerClick(0.0, 10.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    const auto& clickAnim = pet.states[0].clickActions[0].animation;
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[0]);
    return true;
}

bool AmbientActionNeverInterruptsClick() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(!controller.TriggerAmbientAction(0.0, 10.0));  // no-op -- click outranks ambient
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    return true;
}

bool WeightedSelectionBoundaryAt70Percent() {
    PetDefinition pet = MakeNormalPetFixture();
    const auto& actions = pet.states[0].ambientActions;  // [0]=0.7 breathing, [1]=0.3 groom
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.0) == 0);
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.69) == 0);
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.70) == 1);  // exact cutoff
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.99) == 1);
    return true;
}

bool WeightedSelectionFallsBackUniformlyWhenWeightsEqual() {
    std::vector<WeightedAction> actions(2);
    actions[0].weight = 1.0;
    actions[1].weight = 1.0;
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.49) == 0);
    NIMVLETS_CHECK(ChooseWeightedActionIndex(actions, 0.51) == 1);
    return true;
}

bool DirectionChangeWhileBaseUpdatesFrameImmediately() {
    PetDefinition pet = MakeNormalPetFixture();
    content::DirectionalAnimationOverride leftBase;
    leftBase.direction = Direction::kLeft;
    leftBase.animation = pet.states[0].baseAnimation;
    leftBase.animation.frames[0].pixels[0] = 0xAB;
    pet.states[0].baseAnimationDirectionOverrides = {leftBase};

    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kRight);
    NIMVLETS_CHECK(controller.SetDirection(Direction::kLeft, 0.0));
    NIMVLETS_CHECK(controller.CurrentFrame().pixels[0] == 0xAB);
    return true;
}

bool DirectionChangeMidActionIsDeferredUntilBaseReturn() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(!controller.SetDirection(Direction::kLeft, 10.0));  // deferred -- no visual change yet
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kLeft);  // but the direction IS recorded
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);  // gesture in progress, untouched
    return true;
}

}  // namespace

void RegisterAnimationControllerTests(testing::TestRunner& runner) {
    runner.Add("AnimationController.StaticBaseHasNoFutureFrameDeadline", StaticBaseHasNoFutureFrameDeadline);
    runner.Add("AnimationController.OneShotProgressesCorrectly", OneShotProgressesCorrectly);
    runner.Add("AnimationController.ClickCompletesAndReturnsToBase", ClickCompletesAndReturnsToBase);
    runner.Add("AnimationController.RepeatedClickCoalescesInsteadOfRestarting", RepeatedClickCoalescesInsteadOfRestarting);
    runner.Add("AnimationController.ClickInterruptsAmbientAction", ClickInterruptsAmbientAction);
    runner.Add("AnimationController.AmbientActionNeverInterruptsClick", AmbientActionNeverInterruptsClick);
    runner.Add("AnimationController.WeightedSelectionBoundaryAt70Percent", WeightedSelectionBoundaryAt70Percent);
    runner.Add("AnimationController.WeightedSelectionFallsBackUniformlyWhenWeightsEqual", WeightedSelectionFallsBackUniformlyWhenWeightsEqual);
    runner.Add("AnimationController.DirectionChangeWhileBaseUpdatesFrameImmediately", DirectionChangeWhileBaseUpdatesFrameImmediately);
    runner.Add("AnimationController.DirectionChangeMidActionIsDeferredUntilBaseReturn", DirectionChangeMidActionIsDeferredUntilBaseReturn);
}

}  // namespace nimvlets::tests
