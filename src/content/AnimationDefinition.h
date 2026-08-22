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
    kOneShot = 2,  // plays once, then the controller returns to a base state (see AnimationController).
};

// One displayable frame: pixel data plus enough metadata to place it
// consistently and time it correctly. Deliberately pure C++ (no SDL) —
// `pixels` is a plain RGBA8 buffer content::PetPackLoader fills in from
// a compiled pack; the graphics layer uploads it into the single active
// texture (graphics::ActiveFrameTexture) and never mutates it. Desde
// Block 05 (pasada de estabilización, DEC-081) no queda ningún handle
// nativo por-frame: había un `rendererHandle` que guardaba una
// SDL_Texture* por CADA frame, retirado junto con ese modelo.
struct FrameDefinition {
    int width = 0;
    int height = 0;

    // The point (in this frame's own pixel coordinates) that should
    // align to the pet's canvas center when rendered — keeps frames
    // with different internal content placement from visually jittering
    // relative to each other. See docs/ANIMATION_RUNTIME.md.
    core::Point anchor{};

    // Milliseconds this frame stays on screen when its
    // AnimationDefinition uses per-frame durations (AnimationDefinition
    // ::fps == 0). Ignored when fps > 0.
    double durationMs = 0.0;

    // RGBA8, row-major, top-to-bottom, straight alpha.
    // Size must be exactly width * height * 4 bytes.
    std::vector<std::uint8_t> pixels;
};

// Dirección genérica de un Nimvlet direccional. Metadata/estado, no un
// concepto por-pet: un pet sin arte direccional simplemente nunca
// puebla ninguna lista de overrides, y toda resolución de dirección cae
// de forma determinista a la animación canónica — ver ResolveAnimation()
// más abajo. `kRight` es la dirección default de todo el runtime
// (AnimationController arranca ahí).
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

    // Whether a finished kOneShot animation actually transitions
    // (Advance() calls into AnimationController's state machine) or
    // simply holds on its last frame forever. Meaningless for
    // kStatic/kLoop. For a WeightedAction's animation (see below) this
    // is normally true — see WeightedAction::targetStateId for *where*
    // it transitions to.
    bool returnsToIdle = true;

    std::vector<FrameDefinition> frames;

    // How long `frameIndex` stays on screen, in ms. Returns 0.0 for an
    // out-of-range index or a malformed (fps<=0 and durationMs<=0)
    // frame — callers treat 0.0 as "cannot advance" (effectively
    // static), never as "advance instantly forever".
    double FrameDurationMs(std::size_t frameIndex) const;
};

// Una variante de una animación para una dirección distinta de la
// canónica (Direction::kRight, ver el comentario de Direction). Solo
// existen entradas para direcciones con arte propio de verdad. Ver
// ResolveAnimation().
struct DirectionalAnimationOverride {
    Direction direction = Direction::kRight;
    AnimationDefinition animation;
};

// Resuelve qué AnimationDefinition mostrar para `direction`, dada una
// animación canónica y su lista (posiblemente vacía) de overrides
// direccionales. Política de fallback, documentada explícitamente:
//   - Direction::kRight, o cualquier dirección sin entrada dedicada en
//     `overrides`: retorna `canonical`.
//   - Cualquier otra dirección CON una entrada dedicada: retorna esa
//     animación.
// Nunca falla. Genérica — reemplaza las antiguas ResolveIdleAnimation/
// ResolveClickReaction/ResolvePassiveAction (Block 02/04.2): esta única
// función cubre la pose base de un BehaviorState y la animación de
// cualquier WeightedAction, sin importar a qué disparador pertenezcan.
const AnimationDefinition& ResolveAnimation(
    const AnimationDefinition& canonical, const std::vector<DirectionalAnimationOverride>& overrides, Direction direction);

// Un disparo posible desde un trigger (click/ambient/hover) de un
// BehaviorState: qué animación reproducir, con qué peso relativo frente
// a otras entradas del mismo trigger, y a qué estado transicionar
// cuando termine (puede ser el MISMO estado — un "self-loop", el caso
// de un click reaction ordinario o una acción pasiva de un pet normal —
// o uno DISTINTO — una transición real, el caso de Frin's
// sit-to-lie/lie-to-sit). `animation` normalmente es PlaybackKind::
// kOneShot; cuando termina (returnsToIdle == true), el controller
// transiciona a `targetStateId`.
struct WeightedAction {
    std::string id;
    double weight = 1.0;
    std::string targetStateId;
    AnimationDefinition animation;
    std::vector<DirectionalAnimationOverride> directionOverrides;
};

// Elige qué entrada de `actions` disparar, ponderada por su
// WeightedAction::weight. Pura y determinista: `uniformRandom01` es un
// valor en [0,1) que el LLAMADOR provee — esta función nunca genera
// aleatoriedad por sí misma. Reemplaza a
// ChooseWeightedPassiveActionIndex (Block 04.3): la misma política
// 70/30, ahora genérica sobre CUALQUIER lista de WeightedAction (ambient,
// hover, o click), no solo `passiveActions`. Precondición: `actions` no
// está vacío.
std::size_t ChooseWeightedActionIndex(const std::vector<WeightedAction>& actions, double uniformRandom01);

// Un estado de comportamiento con nombre: una pose base (mostrada en
// reposo) más tres triggers posibles desde ahí (ambient/hover/click),
// cada uno una lista ponderada de WeightedAction — posiblemente vacía,
// lo que significa "ese trigger no hace nada mientras el pet está en
// este estado" (p. ej. Frin no tiene ambient mientras está lying, y no
// tiene ningún hover propio en ningún estado todavía).
//
// Un pet "normal" (Bunny, Nidir) tiene exactamente UN BehaviorState: su
// baseAnimation es la pose estática de siempre, clickActions tiene una
// entrada (el click reaction de siempre, self-loop), ambientActions
// tiene las acciones pasivas ponderadas de siempre (self-loop), y
// hoverUsesAmbientActions es true (mismo pool, sin duplicar datos — ver
// su comentario). Un pet "stateful" (Frin) tiene dos o más
// BehaviorState con transiciones reales entre ellos.
struct BehaviorState {
    std::string id;

    AnimationDefinition baseAnimation;
    std::vector<DirectionalAnimationOverride> baseAnimationDirectionOverrides;

    // Segundos objetivo entre disparos ambient mientras el pet está en
    // este estado — sin sentido si ambientActions está vacío (no hay
    // ningún timer armado para un estado sin acciones ambient, ver
    // AnimationController/SpikeApp). Reemplaza al viejo
    // PetDefinition::passiveIntervalSeconds (que era un único valor
    // global por pet) — ahora es por-estado, lo que Frin necesita
    // (rest-delay solo aplica en "seated", nunca en "lying").
    double ambientIntervalSeconds = 300.0;
    std::vector<WeightedAction> ambientActions;

    // Si true, un disparo por hover elige de ambientActions (mismo pool,
    // sin duplicar frames/pixeles en el pack compilado) — la política
    // por defecto para un pet normal ("hover uses the same available
    // passive-action pool unless content says otherwise"). Si false, se
    // usa hoverActions (posiblemente vacío = sin acción de hover en este
    // estado, el caso de Frin hoy). Nunca ambas cosas a la vez — el
    // loader rechaza un pack que ponga hoverUsesAmbientActions=true Y
    // defina hoverActions no vacío, para no dejar ambigüedad sobre cuál
    // gana.
    bool hoverUsesAmbientActions = true;
    std::vector<WeightedAction> hoverActions;

    std::vector<WeightedAction> clickActions;
};

// El pool de acciones que un disparo de hover debe consultar realmente
// para `state` — hoverActions si el estado define su propia lista,
// ambientActions si hoverUsesAmbientActions (el caso normal). Un
// helper trivial, pero centraliza la regla en un solo lugar en vez de
// repetirla en AnimationController y en SpikeApp.
const std::vector<WeightedAction>& EffectiveHoverActions(const BehaviorState& state);

// Everything needed to run one kind of Nimvlet: its behavior graph
// (estados + transiciones) y los thresholds/escala que lo gobiernan.
// One logical Nimvlet — `variantGroup` exists so a pet can express
// "Frin has male/female variants" in data (ver docs/CATALOG.md).
struct PetDefinition {
    std::string id;
    std::string displayName;
    std::string variantGroup;

    int canvasWidth = 160;
    int canvasHeight = 160;

    // Escala visual/de despliegue por-pet (Block 05). Multiplica el
    // tamaño en pantalla (ventana, render, hit-mask) — nunca el arte
    // fuente ni los bytes del pack compilado, y nunca por-animación:
    // TODAS las animaciones de un mismo pet comparten esta única escala
    // (ver SpikeApp::EffectiveCanvasWidth()/EffectiveCanvasHeight()).
    // Default 1.0 — sin cambio de comportamiento para cualquier pet que
    // no la defina explícitamente. Dato de contenido puro: ajustar el
    // tamaño percibido de un pet es cambiar este único número y
    // recompilar, nunca una rama de código por especie.
    double visualScale = 1.0;

    // alpha >= this value counts as visible/interactive (see
    // core::AlphaMask::FromAlphaChannel). Configurable per pet, not a
    // global constant.
    std::uint8_t alphaHitThreshold = 128;

    // El grafo de comportamiento completo. states[0] es el estado
    // inicial/default al arrancar (o al hacer switch a este pet) — ver
    // AnimationController. Todo pet válido tiene al menos un estado
    // (impuesto por PetPackLoader). Cada WeightedAction::targetStateId
    // de cualquier estado DEBE referenciar un id presente en esta misma
    // lista (validado al cargar) — nunca indexar states[] a mano fuera
    // de AnimationController, usar sus accesores.
    std::vector<BehaviorState> states;

    // Target average seconds between passive actions, retenido a nivel
    // de contentVersion solo como metadato de compatibilidad de
    // formato -- el valor real que gobierna el scheduler vive ahora en
    // cada BehaviorState::ambientIntervalSeconds.
    std::string contentVersion;
};

// NOTA histórica para quien agregue una colección de animaciones nueva
// a BehaviorState/PetDefinition: hasta Block 05 hacía falta acordarse
// de cubrir CADA colección nueva en SpikeApp::AttachAllTextures()/
// ReleaseAllTextures(), porque cada frame necesitaba su propia
// SDL_Texture adjuntada por adelantado; olvidarse ahí resolvía bien en
// AnimationController pero renderizaba transparente en silencio (pasó
// de verdad en Block 04.2 -- ver docs/NIDIR_CONTENT.md, "bug de
// cobertura de texturas").
//
// Esa clase de bug ya NO puede ocurrir: graphics::ActiveFrameTexture
// sube el frame que AnimationController esté mostrando, sea cual sea,
// en el momento de dibujarlo (SpikeApp::RenderFrame()) -- no hay
// ninguna lista de colecciones que mantener sincronizada. Una
// colección nueva funciona por construcción, sin tocar el render.

}  // namespace nimvlets::content
