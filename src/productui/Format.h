#pragma once

#include <cstdint>
#include <string>

#include "core/Localization.h"

namespace nimvlets::productui {

// "1 248 clicks" (en) / "1 248 clics" (es) — dígitos agrupados de a
// tres con un espacio, y la palabra click/clic según el número
// (localizada vía core::Localized — brief 06 §6 / 06.1 §16: así, no
// "1,248 COINS" ni "monedas"). Un espacio ASCII como separador de
// grupo: suficiente para la única moneda, sin sobre-ingeniería de
// localización.
std::string FormatClickCount(std::uint64_t clicks, core::Language lang);

// Solo el número agrupado, sin la palabra ("1 248"). Independiente del
// idioma (el separador es un espacio en ambos).
std::string FormatGroupedNumber(std::uint64_t value);

}  // namespace nimvlets::productui
