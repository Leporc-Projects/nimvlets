#pragma once

#include <cstdint>
#include <string_view>

namespace nimvlets::core {

// Política PURA de conteo de clics (Block 11A). Responde una sola
// pregunta, de forma exhaustiva y testeable sin SDL, sin ventana y sin
// permiso del OS: dado el modo que el owner PIDIÓ y si el monitor global
// está REALMENTE activo, ¿esta fuente de clic incrementa el wallet?
//
// Existe como política y no como un puñado de `if (global)` repartidos
// por src/app precisamente porque la regla crítica del bloque —"un clic
// físico sobre el pet nunca vale +2"— es una invariante de producto, no
// un detalle de implementación de un call site. Ver docs/GLOBAL_CLICK_MODE.md.

// --- Modo PEDIDO (la preferencia persistida) ------------------------
//
// Lo que el owner eligió en Settings. NO dice si el conteo global está
// funcionando ahora mismo: eso es el modo EFECTIVO de más abajo.
enum class ClickCountingMode : std::uint8_t {
    // Default de producto y de migración: solo un clic directo sobre el
    // Nimvlet cuenta. Es el estado en el que NINGÚN permiso de input
    // global se pide ni se necesita.
    kNimvletOnly = 0,
    // El owner optó explícitamente por contar cualquier presión del
    // botón primario en el sistema. Que llegue a estar ACTIVO depende de
    // la capacidad de la plataforma y del permiso del OS.
    kAnywhere = 1,
};

// Id persistido y estable (AGENTS.md §17: los strings de formato en
// disco no se traducen). Va a persistence::AppState::clickCountingMode,
// con el mismo idioma de "sizeChoice"/"language".
const char* ClickCountingModeId(ClickCountingMode mode);

// Parsea el id persistido. Vacío o desconocido -> kNimvletOnly. Ese
// fallback ES la invariante de migración del bloque (brief §12): un
// estado v1..v5 no trae el campo, un archivo editado a mano puede traer
// basura, y en los dos casos el resultado seguro y privado es "solo el
// Nimvlet" — nunca se habilita conteo global por accidente de parseo.
ClickCountingMode ParseClickCountingMode(std::string_view id);

// --- Modo EFECTIVO (lo que realmente está pasando) ------------------
//
// Se deriva, nunca se persiste. Un `kAnywhere` pedido cuyo monitor no
// arrancó (permiso denegado/revocado, backend ausente, fallo de startup)
// cae con seguridad a kLocal: los clics sobre el pet SIGUEN contando.
// Nunca existe un estado en el que los clics dejen de contarse en
// silencio (brief §5).
enum class EffectiveClickCounting {
    kLocal,
    kGlobal,
};

// De dónde vino un clic candidato a contar.
enum class ClickSource {
    // Un gesto clasificado como clic sobre la región interactiva del pet
    // (core::DragClassifier -> PointerGesture::kClick).
    kLocalPet,
    // Una presión del botón primario reenviada por el monitor global
    // nativo al hilo principal. Sin coordenadas, sin timestamp, sin
    // app — ver platform::GlobalClickMonitor.
    kGlobalMonitor,
};

EffectiveClickCounting ResolveEffectiveClickCounting(ClickCountingMode requested, bool monitorActive);

// La regla de doble conteo, completa, en un solo lugar (brief §4/§20).
// Las cuatro combinaciones son deliberadamente explícitas:
//
//   (kLocal,  kLocalPet)       -> true   comportamiento histórico
//   (kLocal,  kGlobalMonitor)  -> false  defensivo: un evento reenviado
//                                        que llega DESPUÉS de Stop() no
//                                        puede colarse en el wallet
//   (kGlobal, kLocalPet)       -> false  EL punto del bloque: el monitor
//                                        global también ve el clic sobre
//                                        el pet, así que la ruta local no
//                                        debe sumar otro
//   (kGlobal, kGlobalMonitor)  -> true   la única fuente de moneda
//
// Ojo: esto gobierna SOLO la moneda. La reacción de personalidad del pet
// (animación de click), el hover y el drag siguen exactamente igual en
// los dos modos (brief §22).
bool CountedClickShouldIncrement(EffectiveClickCounting effective, ClickSource source);

}  // namespace nimvlets::core
