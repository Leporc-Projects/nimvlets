#pragma once

#include <cstddef>
#include <optional>

#include "content/AnimationDefinition.h"

namespace nimvlets::content {

// Which of the three required states (block brief §6) is currently
// active. Drag/input handling is *not* a state here — it lives entirely
// in core::DragClassifier and src/app's event handling, which simply
// never calls TriggerClick() for a drag. See docs/ANIMATION_RUNTIME.md
// for how the two interact.
enum class ControllerState {
    kIdle,
    kClickReaction,
    kPassiveAction,
};

// A small, deadline-driven, SDL-free animation state machine for one
// active PetDefinition. Deliberately pure/testable — see
// tests/AnimationControllerTest.cpp — so its timing logic can be
// verified with fabricated timestamps, the same way
// core::FrameScheduler was in Block 01.
//
// Behavioral contract (see docs/ANIMATION_RUNTIME.md for the full
// writeup):
// - Idle plays ResolveIdleAnimation(pet, direction) — `pet.idle` for
//   Direction::kRight or any direction without a dedicated override
//   (see AnimationDefinition.h), otherwise the matching entry in
//   `pet.idleDirectionOverrides` (Block 04.2 — see
//   docs/NIDIR_CONTENT.md). If it's PlaybackKind::kStatic (one frame),
//   NextFrameDeadlineMs() returns nullopt forever — there is never a
//   reason to wake up just to re-check idle's animation.
// - TriggerClick() starts `pet.clickReaction` from frame 0, unless a
//   click reaction is already playing, in which case it is a no-op for
//   *animation* state (repeated clicks never restart the visual) — the
//   caller is responsible for counting the click itself, unconditionally,
//   regardless of what this call does to animation state.
// - TriggerClick() interrupts an in-progress passive action immediately
//   (click reaction outranks passive action — block brief §2D).
// - TriggerPassiveAction() only has an effect when currently Idle; a
//   passive trigger arriving while a click reaction (or another passive
//   action) is playing is silently ignored — passive action never
//   interrupts anything.
// - A finished one-shot animation (returnsToIdle == true) transitions
//   back to Idle automatically the moment Advance() observes it.
class AnimationController {
public:
    explicit AnimationController(const PetDefinition& pet);

    // Advances the current animation according to elapsed time,
    // possibly stepping through more than one frame boundary (or
    // completing a one-shot and returning to Idle) if `nowMs` is far
    // enough past the last call. Safe — and a no-op — to call at any
    // time, including while Idle on a static animation.
    //
    // Returns true if the *displayed frame* actually changed (a new
    // frame index, or a state transition), so the caller knows whether
    // a redraw / hit-mask update is actually needed. Never returns true
    // spuriously.
    bool Advance(double nowMs);

    // See class doc comment for the coalescing/interruption rules.
    // `nowMs` is the monotonic timestamp the new gesture started at.
    void TriggerClick(double nowMs);

    // `passiveActionIndex` selects which of pet.passiveActions to play;
    // out-of-range indices are silently ignored (defensive — the
    // scheduler that calls this is expected to pass a valid index from
    // a non-empty pet.passiveActions).
    void TriggerPassiveAction(std::size_t passiveActionIndex, double nowMs);

    // Cambia la dirección activa (Block 04.2 — ver
    // docs/NIDIR_CONTENT.md). Dirección es metadata genérica, no un
    // gesto: nunca interrumpe un ClickReaction/PassiveAction en curso.
    // Si el controller está actualmente Idle, el frame mostrado se
    // actualiza de inmediato a ResolveIdleAnimation(pet, direction),
    // frame 0 (misma semántica de "reiniciar en frame 0" que
    // TriggerClick()/TriggerPassiveAction() ya usan al empezar una
    // animación nueva). Si NO está Idle, la nueva dirección queda
    // guardada y se aplica recién la próxima vez que
    // TransitionToIdle() corra — nunca se pierde, nunca se aplica a
    // medias.
    //
    // Retorna true si el frame mostrado cambió de verdad (para que el
    // llamador sepa si hace falta un redraw) — false si `direction` ya
    // era la activa, o si el controller no está Idle ahora mismo (el
    // cambio quedó guardado pero no hay nada nuevo que dibujar todavía).
    bool SetDirection(Direction direction, double nowMs);

    Direction CurrentDirection() const { return direction_; }

    ControllerState State() const { return state_; }
    const FrameDefinition& CurrentFrame() const;

    // The timestamp (same clock/epoch as `nowMs` everywhere in this
    // class) at which Advance() next has real work to do — nullopt when
    // the currently playing animation is PlaybackKind::kStatic, which is
    // exactly the signal src/app's event loop uses to block indefinitely
    // instead of waking on a timer (see docs/ANIMATION_RUNTIME.md,
    // "scheduler behavior").
    std::optional<double> NextFrameDeadlineMs() const;

private:
    void TransitionToIdle(double nowMs);

    const PetDefinition& pet_;

    // Dirección activa — ver SetDirection(). Default kRight, igual que
    // el runtime entero (block brief 04.2 §7: "default Nidir direction
    // = right"). Declarado ANTES que currentAnimation_ a propósito: el
    // constructor inicializa currentAnimation_ leyendo direction_ (vía
    // ResolveIdleAnimation()), y el orden real de inicialización de
    // miembros sigue el orden de DECLARACIÓN, no el orden escrito en la
    // lista de inicialización — si direction_ se declarara después,
    // currentAnimation_ leería un direction_ todavía sin inicializar.
    Direction direction_ = Direction::kRight;

    ControllerState state_ = ControllerState::kIdle;
    const AnimationDefinition* currentAnimation_;
    std::size_t currentFrameIndex_ = 0;
    double currentFrameStartMs_ = 0.0;
};

}  // namespace nimvlets::content
