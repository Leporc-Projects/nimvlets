#pragma once

#include <string>

#include "productui/UiColor.h"

namespace nimvlets::productui {

// Identidad de acento SUTIL por Nimvlet lógico (Block 06.1 §9). NO es
// un tema completo: solo tiñe cosas chicas — la línea de foco/selección
// del hero, el subrayado de la variante activa, y la forma orgánica muy
// tenue detrás del arte del hero. El resto del Product UI NO cambia de
// color por pet (brief §9: "Do NOT recolor the entire Product UI per
// pet").
//
// Pura, sin SDL — vive en nimvlets_productui_core para que
// CollectionLayout la lleve y los tests la verifiquen.
struct PetAccent {
    // Color de identidad, saturación media: para el trazo de foco (2pt)
    // y el subrayado de la variante seleccionada.
    UiColor line;

    // Versión pálida del mismo tono: para la(s) forma(s) del hero stage
    // detrás del arte. La vista además la dibuja con alpha bajo — el
    // stage "apoya" el arte, nunca compite (brief §10/§11).
    UiColor shapeTint;

    // Relleno tenue del botón de acción primario ("Use <pet>"): un tinte
    // claro/medio del tono del pet (Block 06.2 §17 — el botón ya NO es
    // casi-negro). Un poco más saturado que shapeTint.
    UiColor softFill;

    // Versión oscura y legible del mismo tono: el texto (y borde) del
    // botón de acción sobre `softFill`. Contraste >= ~5:1 con softFill.
    UiColor deepInk;

    // Si la forma del hero es más orgánica (óvalo, `false`) o un poco
    // más angular (round-rect de radio moderado, `true`). Bunny/Frin
    // -> orgánica; Nidir -> más angular (brief §10).
    bool angularShape = false;
};

// Acento para `petId` (el id de catálogo: "bunny", "nidir", "frin",
// ...). Un id desconocido -> el acento terracota neutro por defecto
// (nunca falla). Determinista.
//
// Direcciones iniciales de diseño del brief §9: Bunny apricot/crema,
// Nidir violeta apagado, Frin azul hielo. Los demás Nimvlets (Rato,
// Rin Rin, Artu, Kyubi, Sweetie) todavía no tienen arte ni id de
// catálogo confirmado — sus tonos se registran acá para no perder la
// dirección de diseño, con ids TENTATIVOS.
PetAccent PetAccentFor(const std::string& petId);

}  // namespace nimvlets::productui
