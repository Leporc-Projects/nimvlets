#pragma once

#include <string>

#include "core/Localization.h"

namespace nimvlets::productui {

// Copy editorial CORTO por Nimvlet, reservado para el hero de la
// Collection (Block 06.1 §14). Deliberadamente mínimo:
//
//  - `ProvisionalSpecies` es una etiqueta de ESPECIE de una palabra
//    ("Rabbit"/"Black dragon"/"Wolf"), factual, tomada de la prosa de
//    docs/PRD_V1.md §3 ("Bunny (rabbit)", "Nidir (dragon)", "Frin
//    (white/cream wolf)"). NO es lore ni personalidad — el brief §14
//    dice explícitamente "Do not have Claude author the permanent
//    personalities". Está marcada como PROVISIONAL: un bloque futuro
//    con dirección de contenido real la reemplaza (idealmente moviendo
//    esto a datos del catálogo/pack).
//
//  - `ShortDescription` (línea de personalidad de una frase) NO se
//    escribe en este bloque: siempre devuelve "". El hero reserva el
//    espacio para cuando exista.
//
// Puro, sin SDL. Los nombres PROPIOS de pet nunca están acá (no se
// traducen).

// Etiqueta de especie provisional para `petId`, en `lang`. "" si no hay
// una asignada (todo pet sin arte real todavía).
const char* ProvisionalSpecies(const std::string& petId, core::Language lang);

// Siempre "" en Block 06.1 — el hero reserva el espacio, el contenido
// llega después.
const char* ShortDescription(const std::string& petId, core::Language lang);

}  // namespace nimvlets::productui
