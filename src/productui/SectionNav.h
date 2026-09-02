#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Localization.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Las tres secciones del Product UI (Block 07 agregó Shop; Block 08
// agrega Settings). Sin routing más complejo — tres estados y una fila
// de texto arriba, "consistent with the current Collection" (brief 07
// §7 / 08 §9).
enum class ProductSection {
    kCollection,
    kShop,
    kSettings,
};

// Etiqueta localizada de una sección: "Collection"/"Colección",
// "Shop"/"Tienda", "Settings"/"Ajustes". Los nombres de sección SÍ se
// traducen (a diferencia de "Nimvlets" y los nombres propios de pet —
// brief 07 §18 / 08 §13).
const char* SectionLabel(ProductSection section, core::Language lang);

// El focusId ("nav:collection" / "nav:shop" / "nav:settings") de la
// pestaña de `section`, o "" si no es una sección conocida.
const char* NavFocusIdFor(ProductSection section);

// Si `focusId` es una pestaña de navegación de la cabecera compartida,
// escribe su sección destino en `outSection` y devuelve true; si no, deja
// `outSection` intacto y devuelve false.
//
// La cabecera (SectionNav) SIEMPRE dibuja las TRES pestañas en las tres
// secciones, así que CUALQUIER vista que la incruste tiene que poder
// rutear las tres. Bloque 08 agregó `kSettings` pero solo enseñó a
// SettingsView a rutear a ella; CollectionView / ShopView seguían con el
// par Collection/Shop de Block 07, dejando la pestaña "Settings"
// inerte — inalcanzable con mouse o teclado — desde las dos secciones
// donde un usuario realmente arranca. Rutear TODAS las vistas por esta
// única función evita que la tabla de secciones y sus consumidores
// vuelvan a divergir.
bool NavTargetSection(const std::string& focusId, ProductSection& outSection);

// Una pestaña de navegación en la cabecera compartida.
struct SectionTab {
    ProductSection section = ProductSection::kCollection;
    std::string label;      // ya localizada
    UiRect labelAnchor;     // ancla IZQUIERDA del texto (baseline la pone la vista)
    UiRect underline;       // regla de 2pt bajo la pestaña activa (w==0 si no activa)
    UiRect hitRect;         // zona clickeable / wash de hover / anillo de foco
    std::string focusId;    // "nav:collection" / "nav:shop" / "nav:settings"
    bool active = false;
};

// La cabecera COMPARTIDA por Collection, Shop y Settings: el título de
// marca "Nimvlets", el balance de clics a la derecha, y la fila de
// pestañas "Collection · Shop · Settings". Cada sección la incrusta en
// su propio layout e inicia su contenido en `bodyTop` — así el texto de
// navegación se ve idéntico en las tres y el foco/hit-test de las
// pestañas es uniforme.
struct SectionHeaderLayout {
    UiRect titleAnchor;         // "Nimvlets" (izquierda) — marca, NO traducida
    UiRect clicksAnchorRight;   // borde DERECHO del "312 clicks" / "312 clics"
    // El balance de clics YA FORMATEADO ("312 clicks" / "312 clics") a
    // partir del balance CANÓNICO que ProductWindow posee. Las CUATRO
    // secciones dibujan este string tal cual — ninguna elige su propio
    // valor (corrección de QA del owner, Block 10: Settings mostraba
    // "0 clicks" hard-codeado).
    std::string clicksText;
    std::vector<SectionTab> tabs;  // siempre [Collection, Shop, Settings] en ese orden
    float bodyTop = 0.0f;       // y (lógica, con scroll ya aplicado) donde empieza el cuerpo
};

// Construye la cabecera. `scrollY` se resta de todas las y (la vista la
// dibuja fuera de su clip de scroll, igual que la Collection de Block
// 06). `clickBalance` es el balance CANÓNICO (de ProductWindow) — se
// formatea acá a `clicksText`. Puro y determinista.
SectionHeaderLayout BuildSectionHeaderLayout(
    float viewportW, float marginX, float scrollY, ProductSection active, core::Language lang,
    std::uint64_t clickBalance);

// --- Métricas de la pill del wallet (Block 12A) ---------------------
//
// El wallet pasa de texto suelto a una cápsula discreta con un spark
// procedural + el balance formateado (referencia B). El ancho depende
// del ancho REAL del texto, que solo la vista puede medir; esta
// función pura arma la geometría a partir de esa medida para que el
// test la fije y la vista y el test no puedan divergir. Puntos
// lógicos. `textWidth` = ancho del string ya formateado
// (SectionHeaderLayout::clicksText).
struct WalletPillMetrics {
    float width = 0.0f;   // ancho total de la pill
    float height = 0.0f;  // alto de la pill
    float sparkCenterX = 0.0f;  // relativo al borde IZQUIERDO de la pill
    float textLeftX = 0.0f;     // relativo al borde IZQUIERDO de la pill (ancla izquierda del balance)
};
WalletPillMetrics ComputeWalletPill(float textWidth);

}  // namespace nimvlets::productui
