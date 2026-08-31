#pragma once

#include <string>
#include <vector>

#include "core/Preferences.h"
#include "productui/SectionNav.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// El layout PURO de la sección Settings (Block 08): convierte
// core::Preferences (tamaño / opacidad / lock / idioma — las MISMAS
// cuatro preferencias que el menú rápido ya expone) + tamaño de
// viewport + scroll en un puñado de grupos con controles segmentados,
// el orden de tabulación, y un hit-test por punto. Métricas en PUNTOS
// lógicos. Determinista: mismas entradas -> mismo resultado, sin SDL,
// sin medición de texto real.
//
// Comparte la CABECERA (título "Nimvlets" + balance + pestañas
// Collection · Shop · Settings) con las otras dos secciones vía
// SectionNav. NADA acá conoce un pet: Settings no usa acento por pet ni
// previews (brief §10/§23). Composición "quiet, warm, desktop-native,
// compact" — grupos separados por espacio y una regla sutil, no cards
// (brief §10).
//
// El FOCO de teclado vive en la FILA (`row:size` ...), no en cada
// segmento: Tab/Shift+Tab recorre nav + filas; ← → cambian el valor de
// la fila enfocada (brief §18, "arrow-key behavior for grouped
// options"). El hit-test SÍ devuelve el segmento exacto ("opt:size:
// small" ...) para que un click de mouse elija esa opción directo.

struct SettingsSegment {
    std::string label;      // ya localizado ("Small" / "Pequeño" / "70%" / "On" ...)
    UiRect rect;            // zona clickeable + pill de selección
    // "opt:<field>:<value>" — p. ej. "opt:size:large", "opt:opacity:70",
    // "opt:lock:on", "opt:language:es". Solo para hit-test de mouse: el
    // FOCO de teclado vive en "row:<field>".
    std::string focusId;
    bool selected = false;  // = es el valor actual de la preferencia
};

struct SettingsRow {
    core::PreferenceField field = core::PreferenceField::kSize;
    std::string label;      // "Size" / "Opacity" / "Lock position" / "Language" (localizado)
    UiRect labelAnchor;     // ancla IZQUIERDA del texto de la fila
    std::vector<SettingsSegment> segments;
    std::string focusId;    // "row:<field>" — el punto de foco de teclado
    UiRect focusRect;       // anillo de foco alrededor del grupo de segmentos
    std::string hint;       // frase corta bajo la fila ("" salvo Lock position)
    UiRect hintAnchor;      // ancla IZQUIERDA del hint (.w = ancho de envoltura)
};

struct SettingsGroup {
    std::string title;      // "Companion" / "Language" (localizado)
    UiRect titleAnchor;
    UiRect rule;            // hairline bajo el título, ancho de contenido
    std::vector<SettingsRow> rows;
};

struct SettingsLayout {
    UiRect viewport;
    SectionHeaderLayout header;
    std::vector<SettingsGroup> groups;

    // Orden de tabulación: pestañas de nav, luego "row:size",
    // "row:opacity", "row:lock", "row:language" (de arriba hacia abajo).
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;

    // focusId accionable en (x, y): una pestaña de nav, o el segmento
    // exacto ("opt:<field>:<value>"). "" si nada.
    std::string HitTest(float x, float y) const;

    // La fila de `field`, o nullptr.
    const SettingsRow* FindRow(core::PreferenceField field) const;
};

struct SettingsLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Preferences prefs;  // idioma incluido (prefs.language)
};

// Construye el layout de Settings. Puro y determinista. Todo el texto
// traducible ya viene localizado según `in.prefs.language`.
SettingsLayout BuildSettingsLayout(const SettingsLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)] (idéntico a
// productui::ClampScroll — se reexpone para no obligar a la vista a
// incluir CollectionLayout.h).
float ClampSettingsScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
