#include "DirectionTest.h"

#include "content/AnimationController.h"
#include "core/AlphaMask.h"

// Tests puros -- sin SDL, sin filesystem -- de la extensión direccional
// del content model: content::ResolveAnimation() (genérica sobre
// cualquier animación canónica + su lista de overrides, ver Block 05 —
// reemplaza a las viejas ResolveIdleAnimation/ResolveClickReaction/
// ResolvePassiveAction, una por colección) y
// content::AnimationController::SetDirection(). Un pet no direccional
// (Bunny) queda cubierto implícitamente: cualquier lista de overrides
// vacía ya se comporta como antes.

using nimvlets::content::AnimationController;
using nimvlets::content::AnimationDefinition;
using nimvlets::content::ControllerMode;
using nimvlets::content::Direction;
using nimvlets::content::DirectionalAnimationOverride;
using nimvlets::content::FrameDefinition;
using nimvlets::content::PetDefinition;
using nimvlets::content::PlaybackKind;
using nimvlets::content::ResolveAnimation;
using nimvlets::core::AlphaMask;

namespace nimvlets::tests {

namespace {

FrameDefinition MakeFrame(int width, int height, std::uint8_t fillByte) {
    FrameDefinition frame;
    frame.width = width;
    frame.height = height;
    frame.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, fillByte);
    return frame;
}

// Pet con la forma REAL de un pet direccional normal (Nidir/Bunny-
// shaped): pose base ESTÁTICA (con override left), un click_reaction
// ONE-SHOT (con override left), y un ambient action ONE-SHOT (con
// override left) -- fill bytes distintos por variante para confirmar
// sin ambigüedad cuál terminó activa.
PetDefinition MakeProductionShapedDirectionalPet() {
    PetDefinition pet;
    pet.id = "directional_pet";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;

    content::BehaviorState state;
    state.id = "default";

    state.baseAnimation.kind = PlaybackKind::kStatic;
    state.baseAnimation.frames = {MakeFrame(4, 4, 0x01)};
    DirectionalAnimationOverride baseLeft;
    baseLeft.direction = Direction::kLeft;
    baseLeft.animation.kind = PlaybackKind::kStatic;
    baseLeft.animation.frames = {MakeFrame(4, 4, 0x02)};
    state.baseAnimationDirectionOverrides = {baseLeft};

    content::WeightedAction click;
    click.id = "click";
    click.targetStateId = "default";
    click.animation.kind = PlaybackKind::kOneShot;
    click.animation.returnsToIdle = true;
    click.animation.frames = {MakeFrame(4, 4, 0x10), MakeFrame(4, 4, 0x11)};
    click.animation.frames[0].durationMs = 50.0;
    click.animation.frames[1].durationMs = 50.0;
    DirectionalAnimationOverride clickLeft;
    clickLeft.direction = Direction::kLeft;
    clickLeft.animation.kind = PlaybackKind::kOneShot;
    clickLeft.animation.returnsToIdle = true;
    clickLeft.animation.frames = {MakeFrame(4, 4, 0x20), MakeFrame(4, 4, 0x21)};
    clickLeft.animation.frames[0].durationMs = 50.0;
    clickLeft.animation.frames[1].durationMs = 50.0;
    click.directionOverrides = {clickLeft};
    state.clickActions = {click};

    content::WeightedAction ambient;
    ambient.id = "ambient";
    ambient.targetStateId = "default";
    ambient.animation.kind = PlaybackKind::kOneShot;
    ambient.animation.returnsToIdle = true;
    ambient.animation.frames = {MakeFrame(4, 4, 0x30), MakeFrame(4, 4, 0x31)};
    ambient.animation.frames[0].durationMs = 50.0;
    ambient.animation.frames[1].durationMs = 50.0;
    DirectionalAnimationOverride ambientLeft;
    ambientLeft.direction = Direction::kLeft;
    ambientLeft.animation.kind = PlaybackKind::kOneShot;
    ambientLeft.animation.returnsToIdle = true;
    ambientLeft.animation.frames = {MakeFrame(4, 4, 0x40), MakeFrame(4, 4, 0x41)};
    ambientLeft.animation.frames[0].durationMs = 50.0;
    ambientLeft.animation.frames[1].durationMs = 50.0;
    ambient.directionOverrides = {ambientLeft};
    state.ambientActions = {ambient};
    state.ambientIntervalSeconds = 12.0;  // matches product policy -- ver DEC-084

    pet.states = {state};
    return pet;
}

// Pet no direccional (forma Bunny antes de tener overrides): ninguna
// lista de overrides poblada.
PetDefinition MakeNonDirectionalPet() {
    PetDefinition pet;
    pet.id = "no_direction_pet";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;
    content::BehaviorState state;
    state.id = "default";
    state.baseAnimation.kind = PlaybackKind::kStatic;
    state.baseAnimation.frames = {MakeFrame(4, 4, 0x10)};
    content::WeightedAction click;
    click.id = "click";
    click.targetStateId = "default";
    click.animation.kind = PlaybackKind::kOneShot;
    click.animation.returnsToIdle = true;
    click.animation.frames = {MakeFrame(4, 4, 0xFF)};
    state.clickActions = {click};
    pet.states = {state};
    return pet;
}

bool ResolveAnimationReturnsCanonicalForRight() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    const content::BehaviorState& state = pet.states[0];
    NIMVLETS_CHECK(&ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, Direction::kRight) == &state.baseAnimation);
    return true;
}

bool ResolveAnimationReturnsOverrideForLeftWhenPresent() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    const content::BehaviorState& state = pet.states[0];
    const AnimationDefinition& resolved =
        ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, Direction::kLeft);
    NIMVLETS_CHECK(&resolved == &state.baseAnimationDirectionOverrides[0].animation);
    NIMVLETS_CHECK(resolved.frames[0].pixels[0] == 0x02);
    return true;
}

// Un pet SIN ningún override (Bunny antes de tener arte direccional)
// pedido en kLeft cae de forma determinista a la canónica -- nunca
// falla, nunca retorna una animación vacía/inválida.
bool ResolveAnimationFallsBackWhenDirectionMissing() {
    const PetDefinition pet = MakeNonDirectionalPet();
    const content::BehaviorState& state = pet.states[0];
    NIMVLETS_CHECK(
        &ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, Direction::kLeft) == &state.baseAnimation);
    return true;
}

bool AnimationControllerStartsAtRightDirectionByDefault() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kRight);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    return true;
}

bool SetDirectionWhileBaseSwitchesImmediately() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);

    const bool changed = controller.SetDirection(Direction::kLeft, 0.0);
    NIMVLETS_CHECK(changed);
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kLeft);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimationDirectionOverrides[0].animation.frames[0]);
    return true;
}

bool SetDirectionToSameDirectionIsANoOp() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    NIMVLETS_CHECK(!controller.SetDirection(Direction::kRight, 0.0));  // ya era kRight
    return true;
}

// Cambiar de dirección mientras hay un ClickAction en curso NO debe
// interrumpirlo (dirección es metadata, no un gesto) -- el cambio
// queda guardado y se aplica recién cuando el controller vuelva a
// Base por su cuenta.
bool SetDirectionDuringClickActionIsDeferred() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    const FrameDefinition* frameBeforeDirectionChange = &controller.CurrentFrame();

    const bool changed = controller.SetDirection(Direction::kLeft, 10.0);
    NIMVLETS_CHECK(!changed);  // no hay redraw que pedir todavía
    NIMVLETS_CHECK(controller.CurrentDirection() == Direction::kLeft);  // pero SÍ quedó guardada
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);  // sin interrumpir el click
    NIMVLETS_CHECK(&controller.CurrentFrame() == frameBeforeDirectionChange);  // frame sin cambios todavía
    return true;
}

// Alternar muchas veces no acumula nada -- cada llamado deja
// CurrentFrame() apuntando exactamente al frame 0 de la dirección
// activa, nunca a un frame index residual de la vez anterior.
bool RepeatedDirectionChangesDoNotAccumulateState() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    const content::BehaviorState& state = pet.states[0];

    for (int i = 0; i < 50; ++i) {
        const Direction target = (i % 2 == 0) ? Direction::kLeft : Direction::kRight;
        controller.SetDirection(target, static_cast<double>(i));
        const AnimationDefinition& expected = ResolveAnimation(state.baseAnimation, state.baseAnimationDirectionOverrides, target);
        NIMVLETS_CHECK(&controller.CurrentFrame() == &expected.frames[0]);
        NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    }
    return true;
}

// El canvas del pet no cambia con la dirección -- AlphaMask::
// FromAlphaChannel siempre produce Width()/Height() == canvasWidth/
// canvasHeight sin importar de qué frame (con qué resolución nativa)
// venga el alpha, incluso si las variantes por dirección tuvieran
// distinta resolución nativa entre sí.
bool HitMaskDimensionsStayConsistentAcrossDirectionSwitch() {
    PetDefinition pet = MakeProductionShapedDirectionalPet();
    pet.states[0].baseAnimationDirectionOverrides[0].animation.frames = {MakeFrame(2, 2, 0xB0)};
    pet.states[0].baseAnimationDirectionOverrides[0].animation.kind = PlaybackKind::kStatic;

    AnimationController controller(pet);
    const AlphaMask maskRight = AlphaMask::FromAlphaChannel(
        controller.CurrentFrame().pixels.data(), controller.CurrentFrame().width, controller.CurrentFrame().height,
        pet.canvasWidth, pet.canvasHeight, 128);
    NIMVLETS_CHECK(maskRight.Width() == pet.canvasWidth && maskRight.Height() == pet.canvasHeight);

    controller.SetDirection(Direction::kLeft, 0.0);
    const AlphaMask maskLeft = AlphaMask::FromAlphaChannel(
        controller.CurrentFrame().pixels.data(), controller.CurrentFrame().width, controller.CurrentFrame().height,
        pet.canvasWidth, pet.canvasHeight, 128);
    NIMVLETS_CHECK(maskLeft.Width() == pet.canvasWidth && maskLeft.Height() == pet.canvasHeight);
    NIMVLETS_CHECK(maskLeft.Width() == maskRight.Width() && maskLeft.Height() == maskRight.Height());
    return true;
}

// La pose base NUNCA debe tener ningún deadline de frame, sin importar
// qué dirección esté activa.
bool StaticBaseHasNoFrameDeadlineRegardlessOfDirection() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controllerRight(pet);
    NIMVLETS_CHECK(!controllerRight.NextFrameDeadlineMs().has_value());
    NIMVLETS_CHECK(!controllerRight.Advance(1'000'000.0));

    AnimationController controllerLeft(pet);
    controllerLeft.SetDirection(Direction::kLeft, 0.0);
    NIMVLETS_CHECK(!controllerLeft.NextFrameDeadlineMs().has_value());
    NIMVLETS_CHECK(!controllerLeft.Advance(1'000'000.0));
    return true;
}

bool ClickActionUsesDirectionalOverrideWhenTriggered() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    controller.SetDirection(Direction::kLeft, 0.0);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].clickActions[0].directionOverrides[0].animation.frames[0]);
    return true;
}

// Avanzar más allá de la duración completa del click action vuelve a
// la pose base ESTÁTICA correcta para la dirección activa.
bool ClickActionReturnsToStaticBaseAfterCompleting() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    controller.SetDirection(Direction::kLeft, 0.0);
    controller.TriggerClick(0.0, 0.0);
    NIMVLETS_CHECK(controller.Advance(100.0));  // 2 frames @ 50ms = 100ms, termina justo
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimationDirectionOverrides[0].animation.frames[0]);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

bool AmbientActionReturnsToStaticBaseAfterCompleting() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    NIMVLETS_CHECK(controller.Advance(100.0));  // 2 frames @ 50ms
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);  // base estática RIGHT (default)
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

// Click interrumpe una acción ambient en curso limpiamente, y vuelve a
// la base estática después -- combina ambos contratos.
bool ClickInterruptsAmbientAndReturnsToStaticBaseAfterward() {
    const PetDefinition pet = MakeProductionShapedDirectionalPet();
    AnimationController controller(pet);
    controller.TriggerAmbientAction(0.0, 0.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kAmbientOrHoverAction);
    controller.Advance(25.0);  // a mitad de camino, todavía frame 0

    controller.TriggerClick(0.0, 30.0);
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kClickAction);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].clickActions[0].animation.frames[0]);

    NIMVLETS_CHECK(controller.Advance(130.0));  // 2 frames @ 50ms desde t=30 => termina en t=130
    NIMVLETS_CHECK(controller.Mode() == ControllerMode::kBase);
    NIMVLETS_CHECK(&controller.CurrentFrame() == &pet.states[0].baseAnimation.frames[0]);
    NIMVLETS_CHECK(!controller.NextFrameDeadlineMs().has_value());
    return true;
}

}  // namespace

void RegisterDirectionTests(testing::TestRunner& runner) {
    runner.Add("Direction/ResolveAnimationReturnsCanonicalForRight", ResolveAnimationReturnsCanonicalForRight);
    runner.Add("Direction/ResolveAnimationReturnsOverrideForLeftWhenPresent", ResolveAnimationReturnsOverrideForLeftWhenPresent);
    runner.Add("Direction/ResolveAnimationFallsBackWhenDirectionMissing", ResolveAnimationFallsBackWhenDirectionMissing);
    runner.Add("Direction/AnimationControllerStartsAtRightDirectionByDefault", AnimationControllerStartsAtRightDirectionByDefault);
    runner.Add("Direction/SetDirectionWhileBaseSwitchesImmediately", SetDirectionWhileBaseSwitchesImmediately);
    runner.Add("Direction/SetDirectionToSameDirectionIsANoOp", SetDirectionToSameDirectionIsANoOp);
    runner.Add("Direction/SetDirectionDuringClickActionIsDeferred", SetDirectionDuringClickActionIsDeferred);
    runner.Add("Direction/RepeatedDirectionChangesDoNotAccumulateState", RepeatedDirectionChangesDoNotAccumulateState);
    runner.Add("Direction/HitMaskDimensionsStayConsistentAcrossDirectionSwitch", HitMaskDimensionsStayConsistentAcrossDirectionSwitch);
    runner.Add("Direction/StaticBaseHasNoFrameDeadlineRegardlessOfDirection", StaticBaseHasNoFrameDeadlineRegardlessOfDirection);
    runner.Add("Direction/ClickActionUsesDirectionalOverrideWhenTriggered", ClickActionUsesDirectionalOverrideWhenTriggered);
    runner.Add("Direction/ClickActionReturnsToStaticBaseAfterCompleting", ClickActionReturnsToStaticBaseAfterCompleting);
    runner.Add("Direction/AmbientActionReturnsToStaticBaseAfterCompleting", AmbientActionReturnsToStaticBaseAfterCompleting);
    runner.Add("Direction/ClickInterruptsAmbientAndReturnsToStaticBaseAfterward", ClickInterruptsAmbientAndReturnsToStaticBaseAfterward);
}

}  // namespace nimvlets::tests
