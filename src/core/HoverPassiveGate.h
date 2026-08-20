#pragma once

namespace nimvlets::core {

// Detector puro, sin SDL, de "el cursor acaba de empezar a estar sobre
// la región interactiva del pet" (Block 04.3, corrección post-QA --
// política de hover pedida por el owner: ver docs/ANIMATION_RUNTIME.md,
// "política de hover"). Deliberadamente NO detecta "está sobre la
// región" en general -- eso dispararía en cada muestra de un hover
// sostenido (varios SDL_EVENT_MOUSE_MOTION por segundo mientras el
// mouse tiembla ligeramente sin salir del área), que es exactamente el
// "spam" que el owner pidió evitar. Solo el FLANCO de subida (la
// transición de "no estaba" a "está") cuenta.
//
// Deliberadamente esta clase NO administra ningún cooldown/tiempo --
// eso lo hace SpikeApp reutilizando su MISMO deadline ambiente de
// acción pasiva (nextPassiveDeadlineMs_), compartido entre el disparo
// por timer y el disparo por hover (ver
// SpikeApp::MaybeTriggerHoverPassiveAction() y el comentario del campo
// en SpikeApp.h) -- así, "10 segundos" es un único intervalo real, no
// dos relojes independientes que podrían solaparse. Esta clase solo
// responde una pregunta, y la responde bien: "¿el cursor ACABA de
// entrar?".
//
// Existe como su propia clase diminuta por la misma razón que
// DragClassifier: para poder testear la detección de flanco en
// aislamiento (ver tests/HoverPassiveGateTest.cpp), sin SDL, sin
// ventana, sin timing real -- src/app/SpikeApp.cpp solo le alimenta un
// booleano por muestra (ya excluyendo cualquier click/drag en curso --
// ver sus call sites) y reacciona a su veredicto.
class HoverPassiveGate {
public:
    // Alimenta una muestra: si el cursor está sobre la región
    // interactiva del pet en este instante. Retorna true exactamente en
    // el flanco de subida -- la muestra donde el hover empieza --,
    // nunca en un hover sostenido ni mientras no hay hover.
    bool Update(bool isHoveringNow);

    // Expuesto sobre todo para tests.
    bool IsHovering() const { return isHovering_; }

private:
    bool isHovering_ = false;
};

}  // namespace nimvlets::core
