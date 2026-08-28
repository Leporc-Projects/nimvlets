#pragma once

#include <string>
#include <vector>

#include "catalog/CollectionModel.h"
#include "core/Localization.h"
#include "productui/PetAccent.h"
#include "productui/UiColor.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Texto humano de estado, localizado (block brief 06 §8 / 06.1 §11):
// nunca badges "ACTIVE"/"LOCKED".
//   kActive        -> "On desktop"    / "En el escritorio"
//   kOwnedInactive -> "Use"           / "Usar"
//   kLocked        -> "Not in your collection" / "No está en tu colección"
const char* StatusText(catalog::OwnershipStatus status, core::Language lang);

// --- Gallery (los Nimvlets NO seleccionados) --------------------------

struct GalleryItem {
    std::string petId;
    std::string displayName;  // nombre propio — nunca traducido
    catalog::OwnershipStatus status = catalog::OwnershipStatus::kLocked;
    std::string statusText;   // localizado
    std::string previewVariantId;  // variante a usar para la preview (Frin: "male"/"female"; resto: "")
    UiColor accentLine;       // tono de identidad del pet (para el foco)
    bool hasVariants = false;

    UiRect cell;     // zona clickeable + wash de hover/foco
    UiRect art;      // caja del arte (chica)
    UiRect name;     // ancla del nombre (centrada en x)
    UiRect status_;  // ancla del estado (centrada en x)

    std::string focusId;  // "item:<petId>"
};

// --- Hero (el Nimvlet seleccionado, protagonista de la pantalla) -----

struct HeroVariantChip {
    std::string variantId;
    std::string label;    // localizado: Male/Female o Macho/Hembra
    UiRect rect;          // zona clickeable (texto + un poco de aire)
    UiRect underline;     // línea de acento de 2pt bajo el chip seleccionado
    std::string focusId;  // "variant:<variantId>"
    bool selected = false;
};

struct CollectionHero {
    std::string petId;
    std::string displayName;  // nombre propio — nunca traducido
    std::string speciesText;  // etiqueta de especie PROVISIONAL, puede ser ""
    catalog::OwnershipStatus status = catalog::OwnershipStatus::kLocked;
    std::string statusText;   // localizado
    PetAccent accent;

    UiRect shape;         // forma orgánica muy tenue detrás del arte
    UiRect art;           // caja del arte grande
    UiRect nameAnchor;    // ancla IZQUIERDA del nombre
    UiRect speciesAnchor;
    UiRect statusAnchor;

    std::vector<HeroVariantChip> variants;  // vacío si el pet no tiene variantes
    std::string selectedVariantId;

    UiRect actionButton;
    std::string actionLabel;    // localizado: "Use Frin" / "On desktop" / "Not in your collection"
    bool actionEnabled = false; // false para el pet activo y para locked (sin compra — brief §12)
    std::string actionFocusId;  // "use" (solo en focusOrder si actionEnabled)
};

struct CollectionLayout {
    UiRect viewport;

    UiRect titleAnchor;            // "Nimvlets" (izquierda) — marca, no traducida
    UiRect clicksAnchorRight;      // borde DERECHO del "1 248 clicks" / "1 248 clics"
    UiRect sectionTitleAnchor;     // "Collection" / "Colección"
    UiRect sectionSubtitleAnchor;  // "Your companions" / "Tus compañeros"
    UiRect dividerRect;            // hairline entre el hero y la gallery

    CollectionHero hero;
    std::vector<GalleryItem> gallery;

    // Orden de tabulación: chips de variante del hero, luego "use" (si
    // habilitado), luego "item:<petId>" por cada entrada de la gallery.
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;  // ya con `scrollY` aplicado a las coordenadas

    const GalleryItem* FindGalleryItem(const std::string& petId) const;

    // focusId del widget accionable en (x, y) en coordenadas de
    // viewport, o "" si no hay ninguno.
    std::string HitTest(float x, float y) const;
};

struct CollectionLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;

    // "" => el hero es el pet ACTIVO del modelo.
    std::string selectedPetId;
    // "" => variante por defecto del item seleccionado.
    std::string selectedVariantId;

    // petId de la entrada de gallery bajo el mouse — se dibuja con un
    // micro-lift de 2pt (instantáneo, sin timer — ver
    // docs/PRODUCT_UI.md §11). "" => ninguna.
    std::string hoverPetId;
};

// Construye el layout hero + gallery completo. Puro y determinista:
// mismas entradas -> mismo resultado, sin SDL, sin medición de texto
// real (los anclas de texto son puntos de referencia; la vista mide y
// dibuja). TODO el texto traducible ya viene localizado según
// `in.language`.
CollectionLayout BuildCollectionLayout(const catalog::CollectionModel& model, const CollectionLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)].
float ClampScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
