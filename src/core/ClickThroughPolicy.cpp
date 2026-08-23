#include "core/ClickThroughPolicy.h"

namespace nimvlets::core {

ClickThroughDecision EvaluateClickThrough(bool cursorInsideWindow, bool cursorOverOpaque, bool dragActive) {
    // El mask de hit solo existe dentro de la ventana, así que "sobre
    // un pixel opaco" implica "dentro". Se normaliza acá en vez de
    // confiar en que todo call site lo mantenga coherente.
    const bool inside = cursorInsideWindow || cursorOverOpaque;

    if (dragActive) {
        // Ver el comentario del header: el gesto manda. No hace falta
        // muestrear — durante un drag la ventana NO está en
        // click-through, así que los eventos de mouse reales siguen
        // llegando solos.
        return ClickThroughDecision{false, false};
    }

    return ClickThroughDecision{inside && !cursorOverOpaque, inside};
}

}  // namespace nimvlets::core
