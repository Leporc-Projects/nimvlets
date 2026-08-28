#pragma once

#include <string>
#include <vector>

#include "core/Localization.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Las dos secciones del Product UI (Block 07). NO hay Settings todavía
// (brief §17); tampoco routing más complejo — dos estados y una barra de
// texto arriba, "consistent with the current Collection" (brief §7).
enum class ProductSection {
    kCollection,
    kShop,
};

// Etiqueta localizada de una sección: "Collection"/"Colección",
// "Shop"/"Tienda". Los nombres de sección SÍ se traducen (a diferencia
// de "Nimvlets" y los nombres propios de pet — brief §18).
const char* SectionLabel(ProductSection section, core::Language lang);

// Una pestaña de navegación en la cabecera compartida.
struct SectionTab {
    ProductSection section = ProductSection::kCollection;
    std::string label;      // ya localizada
    UiRect labelAnchor;     // ancla IZQUIERDA del texto (baseline la pone la vista)
    UiRect underline;       // regla de 2pt bajo la pestaña activa (w==0 si no activa)
    UiRect hitRect;         // zona clickeable / wash de hover / anillo de foco
    std::string focusId;    // "nav:collection" / "nav:shop"
    bool active = false;
};

// La cabecera COMPARTIDA por la Collection y el Shop: el título de marca
// "Nimvlets", el balance de clics a la derecha, y la fila de pestañas
// "Collection · Shop". Ambas secciones la incrustan en su propio layout
// e inician su contenido en `bodyTop` — así el texto de navegación se ve
// idéntico en las dos y el foco/hit-test de las pestañas es uniforme.
struct SectionHeaderLayout {
    UiRect titleAnchor;         // "Nimvlets" (izquierda) — marca, NO traducida
    UiRect clicksAnchorRight;   // borde DERECHO del "312 clicks" / "312 clics"
    std::vector<SectionTab> tabs;  // siempre [Collection, Shop] en ese orden
    float bodyTop = 0.0f;       // y (lógica, con scroll ya aplicado) donde empieza el cuerpo
};

// Construye la cabecera. `scrollY` se resta de todas las y (la vista la
// dibuja fuera de su clip de scroll, igual que la Collection de Block
// 06). Puro y determinista.
SectionHeaderLayout BuildSectionHeaderLayout(
    float viewportW, float marginX, float scrollY, ProductSection active, core::Language lang);

}  // namespace nimvlets::productui
