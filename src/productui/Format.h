#pragma once

#include <cstdint>
#include <string>

namespace nimvlets::productui {

// "1 248 clicks" — dígitos agrupados de a tres con un espacio, y la
// palabra "click"/"clicks" según el número (block brief §6: así, no
// "1,248 COINS" ni "💰"). Un espacio ASCII simple como separador de
// grupo: suficiente para la única moneda y el único idioma de este
// bloque, sin sobre-ingeniería de localización (block brief §6).
std::string FormatClickCount(std::uint64_t clicks);

// Solo el número agrupado, sin la palabra ("1 248").
std::string FormatGroupedNumber(std::uint64_t value);

}  // namespace nimvlets::productui
