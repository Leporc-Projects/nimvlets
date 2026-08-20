#include "StatefulBehaviorTest.h"

#include "TestPetFixtures.h"
#include "content/AnimationController.h"

// Cobertura del grafo de comportamiento por-estado (Block 05 — el
// mismo mecanismo genérico que sirve a Frin hoy y a Artu en el futuro,
// sin ninguna rama de código específica de ninguno de los dos — ver
// MakeStatefulPetFixture() en TestPetFixtures.h, con la misma forma
// seated/lying que describe el block brief).

using nimvlets::content::AnimationController;
using nimvlets::content::ControllerMode;
using nimvlets::content::Direction;
using nimvlets::content::PetDefinition;

namespace nimvlets::tests {

namespace {

bool StartsSeated() {
    PetDefinition pet = MakeStatefulPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.CurrentStateId() == "seated");
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool AmbientRestDelayTransitionsSeatedToLying() {
    PetDefinition pet = MakeStatefulPetFixture();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.TriggerAmbientAction(0.0, 0.0));  // sit_to_lie, único ambient de "seated"
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    NIMVLETS_CHECK(controller.CurrentStateId() == "seated");  // todavía en curso -- no cambia hasta terminar

    NIMVLETS_CHECK(controller.Advance(200.0));  // termina el one-shot de 2 frames @ 100ms
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[1].baseAnimation.frames[0]);
    return true;
}

bool NoRandomClickActionsWhileLying() {
    // "No random howl/tail-greet while lying" -- lying.ambientActions
    // está vacío por construcción (MakeStatefulPetFixture), así que un
    // disparo ambient mientras lying es SIEMPRE un no-op, sin importar
    // el random -- nunca hay forma de que howl/tail-greet ocurran ahí.
    PetDefinition pet = MakeStatefulPetFixture();
    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    controller.Advance(200.0);
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");

    NIMVLETS_CHECK(!controller.TriggerAmbientAction(0.5, 300.0));  // no-op -- lying.ambientActions vacío
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool LyingClickTransitionsToSeatedViaLieToSit() {
    PetDefinition pet = MakeStatefulPetFixture();
    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    controller.Advance(200.0);
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");

    NIMVLETS_CHECK(controller.TriggerClick(0.0, 300.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");  // aún en curso

    NIMVLETS_CHECK(controller.Advance(500.0));  // termina lie_to_sit (2 frames @ 100ms == 200ms)
    NIMVLETS_CHECK(controller.CurrentStateId() == "seated");
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    return true;
}

bool SeatedWeightedClickChoosesHowlOrTailGreetAndReturnsToSeated() {
    PetDefinition pet = MakeStatefulPetFixture();

    // rand < 0.7 -> howl (índice 0), self-loop a "seated".
    {
        AnimationController controller(pet);
        NIMVLETS_CHECK(controller.TriggerClick(0.1, 0.0));
        NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].clickActions[0].animation.frames[0]);
        NIMVLETS_CHECK(controller.Advance(100.0));  // termina howl (1 frame @ 100ms)
        NIMVLETS_CHECK(controller.CurrentStateId() == "seated");
        NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    }
    // rand >= 0.7 -> tail_greet (índice 1), también self-loop a "seated".
    {
        AnimationController controller(pet);
        NIMVLETS_CHECK(controller.TriggerClick(0.9, 0.0));
        NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].clickActions[1].animation.frames[0]);
        NIMVLETS_CHECK(controller.Advance(100.0));
        NIMVLETS_CHECK(controller.CurrentStateId() == "seated");
    }
    return true;
}

bool DirectionChangeWhileSeatedAppliesImmediately() {
    PetDefinition pet = MakeStatefulPetFixture();
    content::DirectionalAnimationOverride leftSeated;
    leftSeated.direction = Direction::kLeft;
    leftSeated.animation = pet.states[0].baseAnimation;
    leftSeated.animation.frames[0].pixels[0] = 0x77;
    pet.states[0].baseAnimationDirectionOverrides = {leftSeated};

    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.SetDirection(Direction::kLeft, 0.0));
    NIMVLETS_CHECK(controller.CurrentFrame().pixels[0] == 0x77);
    return true;
}

bool DirectionChangeWhileLyingAppliesImmediately() {
    PetDefinition pet = MakeStatefulPetFixture();
    content::DirectionalAnimationOverride leftLying;
    leftLying.direction = Direction::kLeft;
    leftLying.animation = pet.states[1].baseAnimation;
    leftLying.animation.frames[0].pixels[0] = 0x88;
    pet.states[1].baseAnimationDirectionOverrides = {leftLying};

    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    controller.Advance(200.0);
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");

    NIMVLETS_CHECK(controller.SetDirection(Direction::kLeft, 200.0));
    NIMVLETS_CHECK(controller.CurrentFrame().pixels[0] == 0x88);
    return true;
}

// Un cambio de dirección que llega A MITAD de una transición (p. ej.
// sit-to-lie todavía reproduciéndose) nunca corrompe el frame visible
// ni salta a una pose no relacionada -- se guarda y se aplica recién
// cuando la transición realmente termine, coherente con el estado
// (lying) al que en efecto se transiciona, no con el estado de origen.
bool DirectionChangeDuringTransitionResolvesCoherentlyAfterCompletion() {
    PetDefinition pet = MakeStatefulPetFixture();
    content::DirectionalAnimationOverride leftLying;
    leftLying.direction = Direction::kLeft;
    leftLying.animation = pet.states[1].baseAnimation;
    leftLying.animation.frames[0].pixels[0] = 0x99;
    pet.states[1].baseAnimationDirectionOverrides = {leftLying};

    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);  // arranca sit_to_lie
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);

    NIMVLETS_CHECK(!controller.SetDirection(Direction::kLeft, 50.0));  // en curso -- diferido, sin cambio visual
    // El frame de la transición en sí (que no tiene override direccional
    // propio en este fixture) sigue mostrándose sin corromperse.
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].ambientActions[0].animation.frames[0]);

    NIMVLETS_CHECK(controller.Advance(200.0));  // termina la transición -> "lying"
    NIMVLETS_CHECK(controller.CurrentStateId() == "lying");
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kLeft);
    NIMVLETS_CHECK(controller.CurrentFrame().pixels[0] == 0x99);  // dirección aplicada, coherente con "lying"
    return true;
}

bool InvalidHoverTriggerIsNoOpWhenStateDefinesNoHoverPool() {
    PetDefinition pet = MakeStatefulPetFixture();  // ambos estados: hoverUsesAmbientActions=false, hoverActions vacío
    AnimationController controller(pet);
    NIMVLETS_CHECK(!controller.TriggerHoverAction(0.0, 0.0));
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    return true;
}

bool TwoStatefulPetsWithSameGraphShapeBehaveIdenticallyWithoutIdentityConfusion() {
    // Modela male/female Frin: dos PetDefinition DISTINTAS que
    // comparten el mismo grafo de estados (mismos ids, misma
    // topología), cada una con su propio contenido -- ejercitar ambas
    // en paralelo nunca mezcla el estado interno de la otra.
    PetDefinition male = MakeStatefulPetFixture();
    male.id = "frin_male";
    PetDefinition female = MakeStatefulPetFixture();
    female.id = "frin_female";

    AnimationController maleController(male);
    AnimationController femaleController(female);

    maleController.TriggerAmbientAction(0.0, 0.0);
    maleController.Advance(200.0);
    NIMVLETS_CHECK(maleController.CurrentStateId() == "lying");
    NIMVLETS_CHECK(femaleController.CurrentStateId() == "seated");  // sin afectar a la otra instancia
    return true;
}

}  // namespace

void RegisterStatefulBehaviorTests(testing::TestRunner& runner) {
    runner.Add("StatefulBehavior.StartsSeated", StartsSeated);
    runner.Add("StatefulBehavior.AmbientRestDelayTransitionsSeatedToLying", AmbientRestDelayTransitionsSeatedToLying);
    runner.Add("StatefulBehavior.NoRandomClickActionsWhileLying", NoRandomClickActionsWhileLying);
    runner.Add("StatefulBehavior.LyingClickTransitionsToSeatedViaLieToSit", LyingClickTransitionsToSeatedViaLieToSit);
    runner.Add(
        "StatefulBehavior.SeatedWeightedClickChoosesHowlOrTailGreetAndReturnsToSeated",
        SeatedWeightedClickChoosesHowlOrTailGreetAndReturnsToSeated);
    runner.Add("StatefulBehavior.DirectionChangeWhileSeatedAppliesImmediately", DirectionChangeWhileSeatedAppliesImmediately);
    runner.Add("StatefulBehavior.DirectionChangeWhileLyingAppliesImmediately", DirectionChangeWhileLyingAppliesImmediately);
    runner.Add(
        "StatefulBehavior.DirectionChangeDuringTransitionResolvesCoherentlyAfterCompletion",
        DirectionChangeDuringTransitionResolvesCoherentlyAfterCompletion);
    runner.Add(
        "StatefulBehavior.InvalidHoverTriggerIsNoOpWhenStateDefinesNoHoverPool",
        InvalidHoverTriggerIsNoOpWhenStateDefinesNoHoverPool);
    runner.Add(
        "StatefulBehavior.TwoStatefulPetsWithSameGraphShapeBehaveIdenticallyWithoutIdentityConfusion",
        TwoStatefulPetsWithSameGraphShapeBehaveIdenticallyWithoutIdentityConfusion);
}

}  // namespace nimvlets::tests
