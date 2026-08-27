#pragma once

#include <string>
#include <vector>

#include "catalog/CollectionModel.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Texto humano de estado bajo el arte (block brief §8): nunca badges
// "ACTIVE"/"LOCKED".
//   kActive        -> "On desktop"
//   kOwnedInactive -> "Use"
//   kLocked        -> "Not in your collection"
const char* StatusShortLabel(catalog::OwnershipStatus status);

// Una entrada del grid de la colección (una fila lógica = un Nimvlet).
struct CollectionItemBox {
    std::string petId;
    std::string displayName;
    catalog::OwnershipStatus status = catalog::OwnershipStatus::kLocked;
    bool hasVariants = false;

    UiRect cell;    // toda la celda de la columna (para el wash de hover/foco)
    UiRect art;     // caja del arte
    UiRect name;    // ancla del nombre (texto centrado en x)
    UiRect status_; // ancla del texto de estado (centrado en x)

    std::string focusId;  // "item:<petId>"
};

// Un chip de variante en el panel de detalle (solo Frin hoy).
struct VariantChip {
    std::string variantId;
    std::string label;  // "Male" / "Female"
    UiRect rect;
    std::string focusId;  // "variant:<variantId>"
    bool selected = false;
};

// El panel de detalle expandido (composición, no modal — block brief
// §10). `open` false => el resto de los campos no tienen sentido.
struct CollectionDetail {
    bool open = false;
    std::string petId;
    std::string displayName;
    catalog::OwnershipStatus status = catalog::OwnershipStatus::kLocked;

    UiRect panel;
    UiRect art;
    UiRect nameAnchor;

    std::vector<VariantChip> variants;  // vacío si el pet no tiene variantes
    std::string selectedVariantId;

    UiRect actionButton;
    std::string actionLabel;      // "Use Nidir" / "On desktop" / "Not in your collection"
    bool actionEnabled = false;   // false para el pet activo y para locked
    std::string actionFocusId;    // "use" (solo presente en focusOrder si actionEnabled)
};

struct CollectionLayout {
    UiRect viewport;

    UiRect titleAnchor;         // "Nimvlets" (izquierda)
    UiRect clicksAnchorRight;   // borde DERECHO donde termina "1 248 clicks"
    UiRect sectionLabelAnchor;  // "Collection"

    std::vector<CollectionItemBox> items;
    CollectionDetail detail;

    // Orden de tabulación: "item:<petId>" en orden de grid, luego los
    // "variant:<id>" y "use" del detalle si está abierto (y "use" solo
    // si actionEnabled).
    std::vector<std::string> focusOrder;

    // Alto total del contenido (para acotar el scroll). El layout ya
    // viene con `scrollY` aplicado a las coordenadas y.
    float contentHeight = 0.0f;

    const CollectionItemBox* FindItem(const std::string& petId) const;

    // focusId del widget accionable en (x, y) en coordenadas de
    // viewport, o "" si no hay ninguno. Cubre items del grid, chips de
    // variante y el botón de acción.
    std::string HitTest(float x, float y) const;
};

struct CollectionLayoutInput {
    float viewportW = 760.0f;
    float viewportH = 540.0f;
    float scrollY = 0.0f;

    bool detailOpen = false;
    std::string detailPetId;
    // "" => usar la variante por defecto del item (CollectionItem::
    // selectedVariantId).
    std::string detailSelectedVariantId;
};

// Construye el layout completo. Puro y determinista: mismas entradas ->
// mismo resultado, sin SDL, sin medición de texto real (los anclas de
// texto son puntos de referencia; la vista mide y dibuja).
CollectionLayout BuildCollectionLayout(const catalog::CollectionModel& model, const CollectionLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)].
float ClampScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
