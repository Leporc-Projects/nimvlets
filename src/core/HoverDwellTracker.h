#pragma once

#include <optional>

namespace nimvlets::core {

// Detector de "dwell" (reposo continuo): responde "¿el cursor acaba de
// completar N segundos CONTINUOS sobre la región interactiva del
// pet?" -- reemplaza a la vieja HoverPassiveGate (Block 05, corrección
// de comportamiento: el owner pidió explícitamente que hover NO se
// dispare apenas el cursor entra, sino solo tras permanecer quieto
// encima durante un tiempo real).
//
// Contrato:
// - `Update(isOverOpaque, nowMs)` se alimenta con una muestra por vez
//   (de cualquier fuente: un evento de motion real, o un re-sample
//   activo del cursor -- ver SpikeApp::MaybeTriggerHoverAction()).
//   Retorna true EXACTAMENTE una vez por episodio de dwell continuo,
//   en la muestra donde el umbral se cruza -- nunca antes, nunca en
//   cada muestra posterior mientras el cursor se queda quieto.
// - Si `isOverOpaque` es false en cualquier muestra, el dwell se
//   reinicia por completo (el cursor debe volver a entrar y esperar
//   los N segundos completos de nuevo).
// - `Reset()` reinicia el dwell explícitamente sin depender de una
//   muestra "fuera" -- usado cuando un click, un drag, o un cambio de
//   BehaviorState ocurre mientras el cursor sigue físicamente encima
//   (el owner pidió que estos tres casos también reinicien el
//   contador, no solo salir del área).
//
// Pura, sin SDL, sin reloj propio (nowMs siempre lo provee el
// llamador) -- mismo idioma que DragClassifier/FrameScheduler, testeada
// en aislamiento con timestamps fabricados, sin sleeps reales.
// Umbral de dwell de PRODUCTO, en segundos: cuánto tiempo continuo
// debe quedarse el cursor sobre pixel visible del pet antes de que
// hover dispare una acción.
//
// Vive acá, y no como un número suelto en src/app, para que la CI
// pueda fijarlo (src/app no es una librería testeable -- arrastra SDL y
// una ventana real). SpikeApp::kHoverDwellSeconds lo referencia en vez
// de repetirlo, así que no puede haber dos valores en desacuerdo.
//
// Historial: 5.0 (primera versión) -> 1.0 ("current 5-second dwell is
// too long") -> 0.5 (DEC-084) -> 0.2 (DEC-090) -> 0.4 (DEC-09x, pasada
// de continuidad de frontera, pedido de producto explícito). Cada
// cambio es un único número -- el mecanismo (dwell continuo, reset
// completo en salida/click/drag/cambio de estado, un solo disparo por
// episodio) nunca cambia.
inline constexpr double kDefaultHoverDwellSeconds = 0.4;

class HoverDwellTracker {
public:
    explicit HoverDwellTracker(double dwellSeconds) : dwellSeconds_(dwellSeconds) {}

    bool Update(bool isOverOpaque, double nowMs) {
        if (!isOverOpaque) {
            dwellStartMs_.reset();
            fired_ = false;
            return false;
        }
        if (!dwellStartMs_.has_value()) {
            dwellStartMs_ = nowMs;
            return false;
        }
        if (fired_) {
            return false;  // ya disparó para este episodio -- hace falta salir y volver a entrar
        }
        if (nowMs - *dwellStartMs_ >= dwellSeconds_ * 1000.0) {
            fired_ = true;
            return true;
        }
        return false;
    }

    void Reset() {
        dwellStartMs_.reset();
        fired_ = false;
    }

    // Expuestos sobre todo para tests/diagnóstico.
    bool IsDwelling() const { return dwellStartMs_.has_value() && !fired_; }
    std::optional<double> DwellStartMs() const { return dwellStartMs_; }

private:
    double dwellSeconds_;
    std::optional<double> dwellStartMs_;
    bool fired_ = false;
};

}  // namespace nimvlets::core
