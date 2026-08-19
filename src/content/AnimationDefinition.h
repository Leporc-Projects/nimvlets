#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Geometry.h"

namespace nimvlets::content {

// How an animation's frames advance over time. See
// docs/ANIMATION_RUNTIME.md for the full behavioral contract of each.
enum class PlaybackKind : std::uint8_t {
    kStatic = 0,   // exactly one frame, never advances — no frame deadline ever exists.
    kLoop = 1,     // wraps back to frame 0 after the last frame.
    kOneShot = 2,  // plays once, then the controller returns to Idle (see AnimationController).
};

// One displayable frame: pixel data plus enough metadata to place it
// consistently and time it correctly. Deliberately pure C++ (no SDL) —
// `pixels` is a plain RGBA8 buffer content::PetPackLoader fills in from
// a compiled pack; the graphics layer turns it into a texture and never
// mutates it. `rendererHandle` is the one deliberate exception: an
// opaque slot the graphics layer may attach a native texture handle to
// after loading, so per-frame textures aren't recreated every time the
// same frame is shown again. content:: never reads or interprets it.
struct FrameDefinition {
    int width = 0;
    int height = 0;

    // The point (in this frame's own pixel coordinates) that should
    // align to the pet's canvas center when rendered — keeps frames
    // with different internal content placement (e.g. a squash/stretch
    // transform) from visually jittering relative to each other, even
    // though every frame in this block's dev content happens to share
    // one fixed canvas size. See docs/ANIMATION_RUNTIME.md.
    core::Point anchor{};

    // Milliseconds this frame stays on screen when its
    // AnimationDefinition uses per-frame durations (AnimationDefinition
    // ::fps == 0). Ignored when fps > 0.
    double durationMs = 0.0;

    // RGBA8, row-major, top-to-bottom, straight alpha.
    // Size must be exactly width * height * 4 bytes.
    std::vector<std::uint8_t> pixels;

    // Opaque; owned/interpreted only by the graphics layer (an
    // SDL_Texture*, once attached). nullptr until the graphics layer
    // attaches one.
    void* rendererHandle = nullptr;
};

// Dirección genérica de un Nimvlet direccional (Block 04.2 — ver
// docs/NIDIR_CONTENT.md). Metadata/estado, no un concepto por-pet: un
// pet sin arte direccional (Bunny) simplemente nunca puebla
// `PetDefinition::idleDirectionOverrides`, y toda resolución de
// dirección cae de forma determinista a `PetDefinition::idle` — ver
// ResolveIdleAnimation() más abajo. `kRight` es la dirección default en
// todo el runtime (AnimationController arranca ahí — ver
// AnimationController.h) y la que `idle` representa implícitamente
// cuando un pet sí tiene contenido direccional.
enum class Direction : std::uint8_t {
    kRight = 0,
    kLeft = 1,
};

// Solo para logs/diagnóstico — nunca para lógica de negocio (comparar
// Direction directamente, no su nombre).
const char* ToString(Direction direction);

// An ordered sequence of frames plus how they play back. Stable `id` is
// data, not an enum — new animations never require new C++.
struct AnimationDefinition {
    std::string id;
    PlaybackKind kind = PlaybackKind::kStatic;

    // > 0: every frame shows for 1000/fps ms, frames[i].durationMs is
    //      ignored.
    // == 0: each frame shows for its own frames[i].durationMs.
    double fps = 0.0;

    // Whether AnimationController transitions back to Idle when this
    // (one-shot) animation completes. Meaningless for kStatic/kLoop.
    bool returnsToIdle = true;

    std::vector<FrameDefinition> frames;

    // How long `frameIndex` stays on screen, in ms. Returns 0.0 for an
    // out-of-range index or a malformed (fps<=0 and durationMs<=0)
    // frame — callers treat 0.0 as "cannot advance" (effectively
    // static), never as "advance instantly forever".
    double FrameDurationMs(std::size_t frameIndex) const;
};

// Una variante de `PetDefinition::idle` para una dirección distinta de
// la canónica que `idle` ya representa (ver el comentario de
// Direction). Solo existen entradas para direcciones con arte propio
// de verdad — Nidir (Block 04.2) tiene exactamente una, `kLeft`; un pet
// no direccional (Bunny) no tiene ninguna. Ver ResolveIdleAnimation().
struct DirectionalAnimationOverride {
    Direction direction = Direction::kRight;
    AnimationDefinition animation;
};

// Igual que DirectionalAnimationOverride, pero para una entrada
// específica de `PetDefinition::passiveActions` (una lista, a
// diferencia de `idle`/`clickReaction`) — `passiveActionIndex` dice a
// cuál. Ver ResolvePassiveAction().
struct PassiveActionDirectionalOverride {
    std::size_t passiveActionIndex = 0;
    Direction direction = Direction::kRight;
    AnimationDefinition animation;
};

// Everything needed to run one kind of Nimvlet: its idle look, its
// click reaction, its passive actions, and the thresholds/timing that
// govern them. One logical Nimvlet — `variantGroup` exists so a future
// block can express "Frin has male/female variants" in data, without
// implementing selection/unlocking here (see docs/DECISION_LOG.md).
// AVISO para quien agregue una colección de animaciones nueva acá
// (idle/clickReaction/passiveActions y sus respectivos overrides
// direccionales YA existentes son todas las que hay hoy): cada
// colección de frames de este struct debe estar cubierta tanto por
// `SpikeApp::AttachAllTextures()`/`ReleaseAllTextures()`
// (src/app/SpikeApp.cpp) como por cualquier futuro camino de
// carga/descarga de texturas -- una colección nueva que se olvide ahí
// se resuelve correctamente en `AnimationController` (los `Resolve*()`
// de este archivo no tienen ningún problema) pero se renderiza
// completamente transparente en runtime, en silencio (sin crash, sin
// error visible más allá de un log de advertencia en
// `SpikeApp::RenderFrame()`). Esto fue un bug real, no hipotético:
// `clickReactionDirectionOverrides`/`passiveActionDirectionOverrides`
// (agregados en la segunda pasada de Block 04.2) faltaban en
// `AttachAllTextures()`/`ReleaseAllTextures()` hasta que se detectó al
// importar el click-fire real de Nidir en la tercera pasada -- ver
// docs/NIDIR_CONTENT.md, "bug de cobertura de texturas".
struct PetDefinition {
    std::string id;
    std::string displayName;

    // Empty string = no variant grouping. Non-empty = this pet is one
    // variant among others sharing the same group id (e.g. "frin").
    // Not used for anything yet in this block — schema-only, per the
    // block brief.
    std::string variantGroup;

    int canvasWidth = 160;
    int canvasHeight = 160;

    // alpha >= this value counts as visible/interactive (see
    // core::AlphaMask::FromAlphaChannel). Configurable per pet, not a
    // global constant — docs/ANIMATION_RUNTIME.md documents why 128 is
    // the default.
    std::uint8_t alphaHitThreshold = 128;

    // El idle canónico del pet. Para un pet no direccional (Bunny) es
    // simplemente "el" idle. Para un pet direccional (Nidir) es, por
    // convención, el idle de Direction::kRight — nunca hace falta una
    // entrada explícita en idleDirectionOverrides para kRight, ver
    // ResolveIdleAnimation().
    AnimationDefinition idle;

    // Variantes de idle para direcciones DISTINTAS de la que `idle`
    // arriba ya representa (Block 04.2 — ver docs/NIDIR_CONTENT.md).
    // Vacío para un pet no direccional. Nunca indexar directamente —
    // usar ResolveIdleAnimation().
    std::vector<DirectionalAnimationOverride> idleDirectionOverrides;

    // El click reaction canónico del pet (Direction::kRight por
    // convención, igual que `idle`). Ver clickReactionDirectionOverrides
    // y ResolveClickReaction().
    AnimationDefinition clickReaction;

    // Variantes de clickReaction para direcciones distintas de la
    // canónica (Block 04.2 — ver docs/NIDIR_CONTENT.md). Vacío para un
    // pet no direccional, o mientras no exista arte de click
    // direccional real. Nunca indexar directamente — usar
    // ResolveClickReaction().
    std::vector<DirectionalAnimationOverride> clickReactionDirectionOverrides;

    // Zero or more sparse autonomous actions; AnimationController picks
    // which one to play by index (see TriggerPassiveAction()).
    std::vector<AnimationDefinition> passiveActions;

    // Variantes direccionales de entradas específicas de
    // `passiveActions` (Block 04.2 — ver docs/NIDIR_CONTENT.md). Lista
    // plana en vez de anidada dentro de `passiveActions` a propósito:
    // mantiene el formato binario NVPACK1 puramente aditivo/al final
    // (ver PetPackLoader.cpp) en vez de tener que intercalar bytes
    // nuevos en medio del layout ya existente. Vacío para un pet no
    // direccional. Nunca indexar directamente — usar
    // ResolvePassiveAction().
    std::vector<PassiveActionDirectionalOverride> passiveActionDirectionOverrides;

    // Target average seconds between passive actions. A scheduling
    // target, not a hard guarantee — see docs/ANIMATION_RUNTIME.md.
    double passiveIntervalSeconds = 300.0;

    // Optional; empty string if not set. Schema-only in this block —
    // nothing reads it yet, but the field exists so future content
    // packs have a place to record it without a schema change.
    std::string contentVersion;
};

// Resuelve qué AnimationDefinition de idle mostrar para `direction`
// (block brief 04.2 §6: "active animation resolves using current pet +
// animation id + direction" — "idle" es, en este bloque, la única
// animación con variantes por dirección). Política de fallback,
// documentada explícitamente en vez de dejarla implícita (block brief
// §6: "unsupported direction fails clearly or uses an explicitly
// documented safe fallback"):
//   - Direction::kRight, o cualquier dirección sin entrada dedicada en
//     `pet.idleDirectionOverrides`: retorna `pet.idle`.
//   - Cualquier otra dirección CON una entrada dedicada: retorna esa
//     animación.
// Nunca falla — todo PetDefinition válido (impuesto por
// PetPackLoader/PetDefinition's ctor implícito) tiene al menos `idle`.
const AnimationDefinition& ResolveIdleAnimation(const PetDefinition& pet, Direction direction);

// Igual que ResolveIdleAnimation() pero para clickReaction: retorna la
// entrada de `pet.clickReactionDirectionOverrides` que calza con
// `direction` si existe una, si no cae a `pet.clickReaction`. Mismo
// contrato de "nunca falla, fallback documentado".
const AnimationDefinition& ResolveClickReaction(const PetDefinition& pet, Direction direction);

// Igual que ResolveIdleAnimation() pero para
// `pet.passiveActions[passiveActionIndex]`: retorna la entrada de
// `pet.passiveActionDirectionOverrides` que calza con
// (passiveActionIndex, direction) si existe una, si no cae a
// `pet.passiveActions[passiveActionIndex]`. Precondición: `passiveActionIndex`
// es un índice válido de `pet.passiveActions` — a diferencia de
// AnimationController::TriggerPassiveAction() (que valida el índice
// antes de llegar acá), esta función no revalida el rango.
const AnimationDefinition& ResolvePassiveAction(const PetDefinition& pet, std::size_t passiveActionIndex, Direction direction);

}  // namespace nimvlets::content
