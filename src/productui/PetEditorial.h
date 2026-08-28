#pragma once

#include <string>

#include "core/Localization.h"

namespace nimvlets::productui {

// Copy editorial CORTO por Nimvlet lógico, para el hero de la Collection
// (Block 06.1 §14 -> Block 06.2 §13-§16). Data-driven y localizable: la
// vista pide `Species(petId, lang)` / `ShortDescription(petId, lang)` y
// NUNCA lleva el texto hard-codeado (brief §16). No es un motor de lore
// — es una tabla chica por id de catálogo + idioma.
//
// Los tres Nimvlets con arte real (Bunny, Nidir, Frin) tienen copy
// bilingüe APROBADA por el owner en Block 06.2 (ya no es placeholder de
// DEV). El resto del roster devuelve "" hasta que se le escriba copy
// propia — el hero simplemente omite la línea.
//
// Puro, sin SDL. Los nombres PROPIOS de pet nunca están acá (no se
// traducen).

// Etiqueta de especie de `petId` en `lang` ("Rabbit"/"Conejo",
// "Black dragon"/"Dragón negro", "White wolf"/"Lobo blanco"). "" si no
// hay una asignada.
const char* Species(const std::string& petId, core::Language lang);

// Línea de personalidad de una frase de `petId` en `lang`. "" si no hay
// una asignada (todo pet sin copy aprobada todavía).
const char* ShortDescription(const std::string& petId, core::Language lang);

}  // namespace nimvlets::productui
