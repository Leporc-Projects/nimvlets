#include "AnimationControllerTest.h"

#include "TestPetFixtures.h"
#include "content/AnimationController.h"

#include <string>

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

// Matriz de prioridad completa (Block 05, segunda pasada de corrección
// post-QA -- "verify and leave clearly and unambiguously defined:
// click during a pending passive, hover during a click, passive
// during hover, etc."): click SIEMPRE gana; ambient y hover NUNCA se
// interrumpen entre sí ni a sí mismos (ambos comparten
// ControllerMode::kAmbientOrHoverAction -- el mismo chequeo "mode_ !=
// kBase" en TriggerAmbientAction()/TriggerHoverAction() ya cubre
// ambos casos por construcción, nunca dos implementaciones separadas
// que puedan divergir). Los tests de arriba (ClickInterruptsAmbientAction/
// AmbientActionNeverInterruptsClick) ya cubren el eje click<->ambient;
// estos cuatro completan el eje click<->hover y ambient<->hover.

bool HoverActionNeverInterruptsClick() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(!controller.TriggerHoverAction(0.0, 10.0));  // no-op -- click outranks hover
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    return true;
}

bool ClickInterruptsHoverAction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerHoverAction(0.0, 0.0));  // picks weight-0.7 "breathing"
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);

    NIMVLETS_CHECK(controller.TriggerClick(0.0, 10.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    const auto& clickAnim = pet.states[0].clickActions[0].animation;
    NIMVLETS_CHECK(&controller.CurrentFrame() == &clickAnim.frames[0]);
    return true;
}

bool AmbientActionNeverInterruptsAnInProgressHoverAction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerHoverAction(0.0, 0.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    NIMVLETS_CHECK(!controller.TriggerAmbientAction(0.0, 10.0));  // no-op -- ya hay una acción en curso
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    return true;
}

bool HoverActionNeverInterruptsAnInProgressAmbientAction() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerAmbientAction(0.0, 0.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    NIMVLETS_CHECK(!controller.TriggerHoverAction(0.0, 10.0));  // no-op -- ya hay una acción en curso
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
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


// ---------------------------------------------------------------------
// Block 05, pasada de estabilización: ciclo de vida de acciones
// (terminación reportada) y prioridad del drag (cancelación al estado
// ACTUAL). Todo con timestamps fabricados -- ningún sleep real, por
// AGENTS.md §12.
// ---------------------------------------------------------------------

bool SelfLoopCompletionIsReportedEvenThoughStateIdNeverChanges() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    const std::string stateBefore = controller.CurrentStateId();

    NIMVLETS_CHECK(controller.TriggerClick(0.0, 0.0));
    NIMVLETS_CHECK(!controller.ActionCompletedDuringLastAdvance().has_value());

    // A mitad de la animación: todavía no terminó nada.
    controller.Advance(100.0);
    NIMVLETS_CHECK(!controller.ActionCompletedDuringLastAdvance().has_value());

    // Al completarse el one-shot (300ms de duración total en el fixture).
    controller.Advance(300.0);
    const auto completed = controller.ActionCompletedDuringLastAdvance();
    NIMVLETS_CHECK(completed.has_value());
    NIMVLETS_CHECK(*completed == 300.0);
    // El id de estado NUNCA cambió -- ésta es exactamente la señal que
    // el viejo disparador basado en CurrentStateId() se perdía.
    NIMVLETS_CHECK(controller.CurrentStateId() == stateBefore);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool CompletionSignalIsNotStickyAcrossAdvances() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    controller.Advance(300.0);
    NIMVLETS_CHECK(controller.ActionCompletedDuringLastAdvance().has_value());
    // Un Advance posterior sin terminación debe limpiar la señal.
    controller.Advance(400.0);
    NIMVLETS_CHECK(!controller.ActionCompletedDuringLastAdvance().has_value());
    return true;
}

bool AmbientActionCompletionIsAlsoReported() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerAmbientAction(0.0, 0.0));
    controller.Advance(300.0);
    NIMVLETS_CHECK(controller.ActionCompletedDuringLastAdvance().has_value());
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool HoverActionCompletionIsAlsoReported() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerHoverAction(0.0, 0.0));
    controller.Advance(300.0);
    NIMVLETS_CHECK(controller.ActionCompletedDuringLastAdvance().has_value());
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool CancelClickActionReturnsToBaseOfCurrentState() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    const std::string stateBefore = controller.CurrentStateId();
    controller.TriggerClick(0.0, 0.0);
    controller.Advance(100.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);

    NIMVLETS_CHECK(controller.CancelActionToCurrentState(150.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(controller.CurrentStateId() == stateBefore);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    return true;
}

bool CancelAmbientActionReturnsToBaseOfCurrentState() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    // La accion ambient del fixture dura 2x50ms=100ms en total, asi que
    // 50ms es "a mitad de camino" (cruzo el frame 0 pero no termino).
    controller.Advance(50.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);

    NIMVLETS_CHECK(controller.CancelActionToCurrentState(75.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    return true;
}

bool CancelWhileAlreadyInBaseIsANoOp() {
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(!controller.CancelActionToCurrentState(50.0));  // false -- nada que cancelar
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool CancelledActionLeavesAStaticPoseWithNoPendingFrameDeadline() {
    // El invariante que hace que "cero avance de frame durante el
    // arrastre" sea real: tras cancelar, la pose base estática no tiene
    // ningún deadline pendiente, así que el loop no se despierta ni
    // avanza nada mientras el owner arrastra.
    PetDefinition pet = MakeNormalPetFixture();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    controller.Advance(100.0);
    NIMVLETS_CHECK(controller.NextFrameDeadlineMs().has_value());  // animación en curso

    controller.CancelActionToCurrentState(150.0);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    // Y un Advance muy posterior no mueve nada.
    NIMVLETS_CHECK(!controller.Advance(1'000'000.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
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
    runner.Add("AnimationController.HoverActionNeverInterruptsClick", HoverActionNeverInterruptsClick);
    runner.Add("AnimationController.ClickInterruptsHoverAction", ClickInterruptsHoverAction);
    runner.Add("AnimationController.AmbientActionNeverInterruptsAnInProgressHoverAction", AmbientActionNeverInterruptsAnInProgressHoverAction);
    runner.Add("AnimationController.HoverActionNeverInterruptsAnInProgressAmbientAction", HoverActionNeverInterruptsAnInProgressAmbientAction);
    runner.Add("AnimationController.WeightedSelectionBoundaryAt70Percent", WeightedSelectionBoundaryAt70Percent);
    runner.Add("AnimationController.WeightedSelectionFallsBackUniformlyWhenWeightsEqual", WeightedSelectionFallsBackUniformlyWhenWeightsEqual);
    runner.Add("AnimationController.DirectionChangeWhileBaseUpdatesFrameImmediately", DirectionChangeWhileBaseUpdatesFrameImmediately);
    runner.Add("AnimationController.DirectionChangeMidActionIsDeferredUntilBaseReturn", DirectionChangeMidActionIsDeferredUntilBaseReturn);
    runner.Add("AnimationController.SelfLoopCompletionIsReportedEvenThoughStateIdNeverChanges", SelfLoopCompletionIsReportedEvenThoughStateIdNeverChanges);
    runner.Add("AnimationController.CompletionSignalIsNotStickyAcrossAdvances", CompletionSignalIsNotStickyAcrossAdvances);
    runner.Add("AnimationController.AmbientActionCompletionIsAlsoReported", AmbientActionCompletionIsAlsoReported);
    runner.Add("AnimationController.HoverActionCompletionIsAlsoReported", HoverActionCompletionIsAlsoReported);
    runner.Add("AnimationController.CancelClickActionReturnsToBaseOfCurrentState", CancelClickActionReturnsToBaseOfCurrentState);
    runner.Add("AnimationController.CancelAmbientActionReturnsToBaseOfCurrentState", CancelAmbientActionReturnsToBaseOfCurrentState);
    runner.Add("AnimationController.CancelWhileAlreadyInBaseIsANoOp", CancelWhileAlreadyInBaseIsANoOp);
    runner.Add("AnimationController.CancelledActionLeavesAStaticPoseWithNoPendingFrameDeadline", CancelledActionLeavesAStaticPoseWithNoPendingFrameDeadline);
}

}  // namespace nimvlets::tests
