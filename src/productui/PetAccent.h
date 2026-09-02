#pragma once

#include <string>

#include "productui/UiColor.h"

namespace nimvlets::productui {

// Identidad de acento por Nimvlet lógico. Empezó como un tinte SUTIL
// (Block 06.1 §9) — la línea de foco/selección del hero, el subrayado
// de la variante activa, la forma orgánica muy tenue detrás del arte.
// Block 12A (DEC-143) la vuelve la ÚNICA identidad lógica por pet: un
// registro de primera parte centralizado, en vez de `if (petId ==
// "nidir")` desparramado por las vistas. Sigue sin recolorear el resto
// del Product UI (brief §9: "Do NOT recolor the entire Product UI per
// pet") — los tokens semánticos (productui::tokens) no cambian por pet.
//
// Pura, sin SDL — vive en nimvlets_productui_core para que
// CollectionLayout / ShopLayout la lleven y los tests la verifiquen.
// Un id desconocido o sin identidad -> un neutro cálido seguro con
// TODOS los campos poblados y legibles (nunca crashea, nunca deja
// texto invisible — brief §10).

// Estilo del hero-stage de un pet. Hoy solo distingue orgánico vs.
// facetado (lo que `angularShape` ya decía); es la COSTURA por la que
// un bloque futuro de mundo/hero puede pedir un tratamiento por pet sin
// rediseñar el Product UI ni inventar un campo de catálogo para una
// imagen que todavía no existe (brief §28).
enum class HeroStageStyle {
    kSoftOval,     // óvalo orgánico — Bunny, Frin, el neutro
    kFacetedRound, // round-rect apenas más angular — Nidir (dragón)
};

struct PetAccent {
    // Color de identidad primario, saturación media: trazo de selección
    // de variante, regla de acento bajo el nombre, punto de "On desktop".
    UiColor line;

    // Acento SECUNDARIO del pet (Block 12A): un tono complementario del
    // mismo pet — el lóbulo secundario del hero stage, futuros toques de
    // mundo. Restringido igual que `line`; nunca compite con el texto.
    UiColor secondary;

    // Versión pálida del tono primario: para la(s) forma(s) del hero
    // stage detrás del arte. La vista la dibuja con alpha bajo — el
    // stage "apoya" el arte, nunca compite (brief §10/§11).
    UiColor shapeTint;

    // Relleno tenue del botón primario ("Use <pet>" / "Get <pet>"): un
    // tinte claro/medio del tono del pet (Block 06.2 §17 — el botón ya
    // NO es casi-negro). Un poco más saturado que shapeTint.
    UiColor softFill;

    // Versión oscura y legible del mismo tono: el texto (y borde) del
    // botón primario sobre `softFill`. Contraste >= ~4.5:1 con softFill
    // (verificado en PetAccentTest vía productui::ContrastRatio); si un
    // pet futuro no lo cumpliera, ButtonStyle lo clampa más oscuro.
    UiColor deepInk;

    // Estilo del hero stage (ver HeroStageStyle). `angularShape` se
    // conserva como conveniencia (== heroStage == kFacetedRound) para
    // no tocar las vistas que ya lo consultan.
    HeroStageStyle heroStage = HeroStageStyle::kSoftOval;
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
