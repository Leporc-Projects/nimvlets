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

// "Need 42 more clicks" / "Te faltan 42 clics" — y en singular "Need 1
// more click" / "Te falta 1 clic" (brief §9/§18). Toda la variación de
// idioma sale de dos StringKey (kNeedMoreClicksOne/Many); acá solo se
// elige por el número y se sustituye "{n}" — sin ninguna rama de
// idioma.
std::string FormatNeedMoreClicks(std::uint64_t shortBy, core::Language lang);

// "Spend 300 clicks to add Nidir to your collection?" / "¿Gastar 300
// clics para añadir Nidir a tu colección?" (brief §12). `petName` es un
// nombre propio — se inserta tal cual, nunca traducido. Igual que
// arriba: la plantilla es un StringKey (kSpendPromptOne/Many), acá solo
// se sustituye "{n}" y "{pet}".
std::string FormatSpendPrompt(std::uint64_t price, const std::string& petName, core::Language lang);

// "Make Artu your first Nimvlet?" / "¿Quieres que Artu sea tu primer
// Nimvlet?" (Block 09A onboarding, brief §23). `petName` es un nombre
// propio — se inserta tal cual (para Frin, ya incluye la variante
// elegida si el caller la formatea así). Plantilla:
// StringKey::kOnboardingConfirmStarter; acá solo se sustituye "{pet}".
std::string FormatOnboardingConfirmPrompt(const std::string& petName, core::Language lang);

// Sustituye "{permission}" por el nombre que el SISTEMA OPERATIVO le da
// al permiso ("Input Monitoring" en macOS), que aporta el adapter de
// plataforma vía platform::GlobalClickStatus::permissionName (Block
// 11A). Es lo que deja que la copy de Settings nombre el permiso real
// SIN una sola rama por plataforma en src/productui (brief §18). Con
// `permissionName` vacío el token se borra y la frase sigue leyéndose.
std::string FormatWithPermission(
    core::StringKey key, const std::string& permissionName, core::Language lang);

}  // namespace nimvlets::productui
