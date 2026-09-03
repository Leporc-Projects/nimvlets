#include "productui/PetAccent.h"

namespace nimvlets::productui {

namespace {

// Constructor chico para no depender del orden posicional del agregado
// (Block 12A agregó `secondary` y `heroStage` en el medio). `faceted`
// == round-rect apenas angular (Nidir); si no, óvalo orgánico.
// `emphasis` = tratamiento de acento en CTA / card seleccionada
// (default seguro kSoft — DEC-148).
PetAccent Make(UiColor line, UiColor secondary, UiColor shapeTint, UiColor softFill, UiColor deepInk,
               bool faceted = false, AccentEmphasis emphasis = AccentEmphasis::kSoft) {
    PetAccent a;
    a.line = line;
    a.secondary = secondary;
    a.shapeTint = shapeTint;
    a.softFill = softFill;
    a.deepInk = deepInk;
    a.heroStage = faceted ? HeroStageStyle::kFacetedRound : HeroStageStyle::kSoftOval;
    a.angularShape = faceted;
    a.emphasis = emphasis;
    return a;
}

// Tono neutro por defecto: la terracota cálida del tema base. Para un
// pet desconocido o sin identidad asignada — todos los campos poblados
// y legibles (brief §10: nunca crashea, nunca texto invisible).
PetAccent Neutral() {
    return Make(UiColor{0xB4, 0x6E, 0x3C, 0xFF},   // line
               UiColor{0xC8, 0x9A, 0x6E, 0xFF},    // secondary — terracota más claro
               UiColor{0xEC, 0xDD, 0xCC, 0xFF},    // shapeTint
               UiColor{0xEF, 0xDF, 0xCF, 0xFF},    // softFill
               UiColor{0x7A, 0x4A, 0x28, 0xFF});   // deepInk
}

}  // namespace

PetAccent PetAccentFor(const std::string& petId) {
    // --- Nimvlets con arte real (Block 05/06). Contraste deepInk /
    //     softFill verificado >= ~5:1 (WCAG AA) en PetAccentTest. ---
    if (petId == "bunny") {
        // Apricot / crema cálida; secundario crema profunda.
        return Make(UiColor{0xE0, 0x9B, 0x63, 0xFF}, UiColor{0xC9, 0xA9, 0x84, 0xFF},
                    UiColor{0xF3, 0xE4, 0xD2, 0xFF}, UiColor{0xF0, 0xDA, 0xC0, 0xFF},
                    UiColor{0x6E, 0x45, 0x23, 0xFF});
    }
    if (petId == "nidir") {
        // Amatista / violeta frío; secundario índigo profundo. Dragón ->
        // hero stage facetado + acento HONDO (el CTA violeta saturado y la
        // card seleccionada del concept — DEC-148).
        return Make(UiColor{0x93, 0x86, 0xBE, 0xFF}, UiColor{0x4B, 0x43, 0x74, 0xFF},
                    UiColor{0xE7, 0xE2, 0xEF, 0xFF}, UiColor{0xE1, 0xD9, 0xEE, 0xFF},
                    UiColor{0x40, 0x35, 0x6A, 0xFF}, /*faceted=*/true, AccentEmphasis::kDeep);
    }
    if (petId == "frin") {
        // Azul hielo pálido; secundario plata/azul (lobo blanco).
        return Make(UiColor{0x8F, 0xAF, 0xC4, 0xFF}, UiColor{0xA9, 0xB8, 0xC6, 0xFF},
                    UiColor{0xE3, 0xEC, 0xF1, 0xFF}, UiColor{0xDA, 0xE7, 0xEF, 0xFF},
                    UiColor{0x2F, 0x50, 0x64, 0xFF});
    }

    // --- Nimvlets sin arte todavía — ids TENTATIVOS, tonos del brief
    //     §9, registrados para no perder la dirección de diseño. ---
    if (petId == "rato") {
        return Make(UiColor{0xE0, 0x95, 0x4E, 0xFF}, UiColor{0xC8, 0xA0, 0x78, 0xFF},
                    UiColor{0xF3, 0xE2, 0xCE, 0xFF}, UiColor{0xF0, 0xDB, 0xBE, 0xFF},
                    UiColor{0x6F, 0x45, 0x20, 0xFF});
    }
    if (petId == "rin_rin" || petId == "rinrin") {
        return Make(UiColor{0x5E, 0x8C, 0x63, 0xFF}, UiColor{0x84, 0xA6, 0x88, 0xFF},
                    UiColor{0xDD, 0xE8, 0xDD, 0xFF}, UiColor{0xD3, 0xE4, 0xD3, 0xFF},
                    UiColor{0x2E, 0x4E, 0x33, 0xFF});
    }
    if (petId == "artu") {
        return Make(UiColor{0xB7, 0x8A, 0x4E, 0xFF}, UiColor{0xC9, 0xAB, 0x82, 0xFF},
                    UiColor{0xEE, 0xE4, 0xCF, 0xFF}, UiColor{0xE9, 0xDB, 0xBD, 0xFF},
                    UiColor{0x5A, 0x43, 0x1F, 0xFF});
    }
    if (petId == "kyubi") {
        return Make(UiColor{0x7A, 0x5A, 0x9E, 0xFF}, UiColor{0x4E, 0x3B, 0x6E, 0xFF},
                    UiColor{0xE2, 0xDA, 0xEC, 0xFF}, UiColor{0xDB, 0xCF, 0xEA, 0xFF},
                    UiColor{0x3C, 0x2C, 0x5C, 0xFF}, /*faceted=*/true, AccentEmphasis::kDeep);
    }
    if (petId == "sweetie") {
        return Make(UiColor{0xD0, 0x6A, 0x45, 0xFF}, UiColor{0xC9, 0x8E, 0x77, 0xFF},
                    UiColor{0xF2, 0xDD, 0xD1, 0xFF}, UiColor{0xEF, 0xCF, 0xC0, 0xFF},
                    UiColor{0x6E, 0x33, 0x20, 0xFF});
    }

    return Neutral();
}

}  // namespace nimvlets::productui
