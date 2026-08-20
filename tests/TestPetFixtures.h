#pragma once

// Fixtures compartidos entre tests de content:: (Block 05 — grafo de
// comportamiento por-estado). Header-only, sin SDL, reusado por
// AnimationControllerTest.cpp, ClickAccountingTest.cpp, DirectionTest.cpp
// y StatefulBehaviorTest.cpp — evita repetir la misma construcción de
// PetDefinition/BehaviorState a mano en cada archivo.

#include "content/AnimationController.h"
#include "content/AnimationDefinition.h"

namespace nimvlets::tests {

inline content::FrameDefinition MakeFrame(double durationMs, std::uint8_t fillByte = 0) {
    content::FrameDefinition frame;
    frame.width = 4;
    frame.height = 4;
    frame.durationMs = durationMs;
    frame.pixels.assign(4 * 4 * 4, fillByte);
    return frame;
}

// Un pet "normal" (Bunny/Nidir-shaped): un único BehaviorState ("default"),
// click self-loop, dos ambient actions ponderadas 70/30 (self-loop),
// hover comparte el pool ambient.
//   click_reaction: one-shot, 3 frames @ 100ms cada uno (300ms total).
//   ambient[0] "breathing": one-shot, 2 frames @ 50ms cada uno, peso 0.7.
//   ambient[1] "groom": one-shot, 2 frames @ 50ms cada uno, peso 0.3.
inline content::PetDefinition MakeNormalPetFixture() {
    content::PetDefinition pet;
    pet.id = "test_pet";
    pet.displayName = "Test Pet";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;

    content::BehaviorState state;
    state.id = "default";

    state.baseAnimation.id = "idle";
    state.baseAnimation.kind = content::PlaybackKind::kStatic;
    state.baseAnimation.frames = {MakeFrame(0.0)};

    content::WeightedAction click;
    click.id = "click_reaction";
    click.weight = 1.0;
    click.targetStateId = "default";
    click.animation.id = "click_reaction";
    click.animation.kind = content::PlaybackKind::kOneShot;
    click.animation.returnsToIdle = true;
    click.animation.frames = {MakeFrame(100.0), MakeFrame(100.0), MakeFrame(100.0)};
    state.clickActions = {click};

    content::WeightedAction breathing;
    breathing.id = "breathing";
    breathing.weight = 0.7;
    breathing.targetStateId = "default";
    breathing.animation.id = "breathing";
    breathing.animation.kind = content::PlaybackKind::kOneShot;
    breathing.animation.returnsToIdle = true;
    breathing.animation.frames = {MakeFrame(50.0), MakeFrame(50.0)};

    content::WeightedAction groom;
    groom.id = "groom";
    groom.weight = 0.3;
    groom.targetStateId = "default";
    groom.animation.id = "groom";
    groom.animation.kind = content::PlaybackKind::kOneShot;
    groom.animation.returnsToIdle = true;
    groom.animation.frames = {MakeFrame(50.0), MakeFrame(50.0)};

    state.ambientActions = {breathing, groom};
    state.ambientIntervalSeconds = 15.0;
    state.hoverUsesAmbientActions = true;

    pet.states = {state};
    return pet;
}

// Un pet "stateful" (Frin-shaped): dos BehaviorState, "seated" y
// "lying", con transiciones reales entre ellos — ver docs del block
// brief. seated: ambient = sit-to-lie (-> lying); click = 70% howl /
// 30% tail-greet (self-loop). lying: sin ambient, sin hover; click =
// lie-to-sit (-> seated).
inline content::PetDefinition MakeStatefulPetFixture() {
    content::PetDefinition pet;
    pet.id = "test_frin";
    pet.displayName = "Test Frin";
    pet.canvasWidth = 4;
    pet.canvasHeight = 4;

    content::BehaviorState seated;
    seated.id = "seated";
    seated.baseAnimation.id = "seated_base";
    seated.baseAnimation.kind = content::PlaybackKind::kStatic;
    seated.baseAnimation.frames = {MakeFrame(0.0, 1)};
    seated.ambientIntervalSeconds = 200.0;  // "rest delay"

    content::WeightedAction sitToLie;
    sitToLie.id = "sit_to_lie";
    sitToLie.weight = 1.0;
    sitToLie.targetStateId = "lying";
    sitToLie.animation.id = "sit_to_lie";
    sitToLie.animation.kind = content::PlaybackKind::kOneShot;
    sitToLie.animation.returnsToIdle = true;
    sitToLie.animation.frames = {MakeFrame(100.0, 2), MakeFrame(100.0, 3)};
    seated.ambientActions = {sitToLie};
    seated.hoverUsesAmbientActions = false;  // sin acción de hover todavía (owner no la definió)

    content::WeightedAction howl;
    howl.id = "howl";
    howl.weight = 0.7;
    howl.targetStateId = "seated";
    howl.animation.id = "howl";
    howl.animation.kind = content::PlaybackKind::kOneShot;
    howl.animation.returnsToIdle = true;
    howl.animation.frames = {MakeFrame(100.0, 4)};

    content::WeightedAction tailGreet;
    tailGreet.id = "tail_greet";
    tailGreet.weight = 0.3;
    tailGreet.targetStateId = "seated";
    tailGreet.animation.id = "tail_greet";
    tailGreet.animation.kind = content::PlaybackKind::kOneShot;
    tailGreet.animation.returnsToIdle = true;
    tailGreet.animation.frames = {MakeFrame(100.0, 5)};

    seated.clickActions = {howl, tailGreet};

    content::BehaviorState lying;
    lying.id = "lying";
    lying.baseAnimation.id = "lying_base";
    lying.baseAnimation.kind = content::PlaybackKind::kStatic;
    lying.baseAnimation.frames = {MakeFrame(0.0, 6)};
    // sin ambientActions -- nunca se arma ningún timer mientras lying.
    lying.hoverUsesAmbientActions = false;  // sin hover tampoco

    content::WeightedAction lieToSit;
    lieToSit.id = "lie_to_sit";
    lieToSit.weight = 1.0;
    lieToSit.targetStateId = "seated";
    lieToSit.animation.id = "lie_to_sit";
    lieToSit.animation.kind = content::PlaybackKind::kOneShot;
    lieToSit.animation.returnsToIdle = true;
    lieToSit.animation.frames = {MakeFrame(100.0, 7), MakeFrame(100.0, 8)};
    lying.clickActions = {lieToSit};

    pet.states = {seated, lying};
    return pet;
}

}  // namespace nimvlets::tests
