#pragma once

namespace nimvlets::core {

// Decisión pura (sin SDL, sin AppKit/Win32/X11) de "¿la ventana debe
// dejar pasar el click al escritorio ahora mismo?" y "¿hace falta
// seguir muestreando la posición del cursor para saberlo?" — Block 05,
// pasada de resolución de click-through en macOS (ver DEC-086 en
// docs/DECISION_LOG.md).
//
// Por qué esto es una decisión con nombre propio y testeable, y no dos
// `if` sueltos dentro de SpikeApp: el mecanismo real de click-through
// (`NSWindow.ignoresMouseEvents` en macOS, `WS_EX_TRANSPARENT` en
// Windows) tiene una propiedad incómoda que gobierna TODO el diseño —
// **mientras el click-through está ACTIVO la ventana deja de recibir
// eventos de mouse**, así que ningún evento puede avisarnos de que el
// cursor volvió sobre el pet. Eso obliga a muestrear el cursor. La
// pregunta de producto real no es "¿muestreamos o no?" sino "¿CUÁNDO
// puede el muestreo cambiar algo observable?".
//
// La observación que hace que el muestreo permanente sea innecesario:
// **si el cursor está FUERA del rectángulo de la ventana, el estado de
// click-through no puede afectar a nadie** — ningún click ahí puede
// llegar a nuestra ventana, esté o no en modo click-through. Así que
// afuera se elige deliberadamente el estado NO-click-through: es
// inobservable para el usuario, y a cambio devuelve la entrega normal
// de eventos de mouse, con lo cual el instante en que el cursor ENTRA
// al rectángulo llega como un evento real (SDL_EVENT_MOUSE_MOTION /
// SDL_EVENT_WINDOW_MOUSE_ENTER) en vez de tener que descubrirlo
// encuestando.
//
// Resultado: el único estado que necesita muestreo periódico es
// "cursor DENTRO del rectángulo de la ventana" — una región chica y una
// fracción chica del tiempo. Con el cursor en cualquier otro lugar de
// la pantalla (el caso ocioso dominante) el loop principal no se
// despierta nunca por click-through. Ver docs/PLATFORM_SPIKE.md para
// la medición de CPU real de esto contra el viejo poll permanente a
// 60Hz.
struct ClickThroughDecision {
    // Estado deseado del mecanismo nativo de click-through: true =
    // "los clicks NO son para nosotros, que pasen a lo que haya
    // debajo".
    bool clickThrough = false;

    // true si el loop principal debe mantener vivo el muestreo
    // periódico del cursor para poder detectar el próximo cambio. false
    // = puede volver a dormirse por completo hasta el próximo evento
    // real.
    bool samplingRequired = false;

    bool operator==(const ClickThroughDecision&) const = default;
};

// `cursorInsideWindow`: el cursor cae dentro del rectángulo de la
//   ventana (no necesariamente sobre un pixel opaco del pet).
// `cursorOverOpaque`: cae sobre la región de hit real derivada del
//   alpha del frame activo (core::AlphaMask). Implica
//   `cursorInsideWindow`; si llegara true con `cursorInsideWindow`
//   false, se trata como "dentro" (el mask solo existe dentro).
// `dragActive`: hay un gesto de arrastre en curso.
//
// Un drag en curso NUNCA es click-through: una vez que el owner
// agarró al pet, soltar el botón sobre un pixel transparente no debe
// hacer que el drag "se caiga" al escritorio a mitad de camino. El
// gesto manda hasta que termine — misma prioridad
// DRAG > CLICK > HOVER/AMBIENT que ya rige el resto del runtime (ver
// DEC-080).
ClickThroughDecision EvaluateClickThrough(bool cursorInsideWindow, bool cursorOverOpaque, bool dragActive);

}  // namespace nimvlets::core
