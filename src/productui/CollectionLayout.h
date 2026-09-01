#pragma once

#include <string>
#include <vector>

#include "catalog/CollectionModel.h"
#include "core/Localization.h"
#include "productui/PetAccent.h"
#include "productui/SectionNav.h"
#include "productui/UiColor.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// Texto humano de estado, localizado. Nunca badges "ACTIVE"/"LOCKED".
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
    std::string previewVariantId;  // variante para la preview (Frin: "male"/"female"; resto: "")
    UiColor accentLine;       // tono de identidad del pet (para el foco de teclado)
    UiColor pedestalTint;     // tinte MUY tenue del pedestal del arte (Block 06.2 §20)
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
    // ¿El owner posee esta variante? Una variante no poseída se muestra
    // en el selector pero atenuada, y NO habilita "Use" (brief §6). En
    // el estado actual del owner tras la migración las dos variantes de
    // Frin están poseídas, así que esto siempre es true para él.
    bool owned = true;
};

struct CollectionHero {
    std::string petId;
    std::string displayName;   // nombre propio — nunca traducido
    std::string speciesText;   // etiqueta de especie ("White wolf"), "" si no hay
    std::string descriptionText;  // línea de personalidad, "" si no hay
    catalog::OwnershipStatus status = catalog::OwnershipStatus::kLocked;
    std::string statusText;    // localizado
    PetAccent accent;

    // Hero stage: composición asimétrica de primitivas de primera parte
    // detrás/alrededor del arte, teñida con accent.shapeTint a alpha
    // bajo (Block 06.2 §11). La vista elige óvalo vs round-rect según
    // accent.angularShape.
    UiRect stagePrimary;    // óvalo/blob grande, se extiende bastante más que el arte
    UiRect stageSecondary;  // primitiva más chica, descentrada hacia el texto
    UiRect art;             // caja del arte grande
    UiRect nameRule;        // línea de acento fina (2pt) bajo el nombre

    UiRect nameAnchor;         // ancla IZQUIERDA del nombre
    UiRect speciesAnchor;
    UiRect descriptionAnchor;
    UiRect statusAnchor;       // solo se dibuja si showStatusLine

    std::vector<HeroVariantChip> variants;  // vacío si el pet no tiene variantes
    std::string selectedVariantId;

    UiRect actionButton;
    std::string actionLabel;    // localizado: "Use Frin"
    bool actionEnabled = false; // true => se dibuja el botón; false => solo el estado
    bool showStatusLine = true; // = !actionEnabled — sin duplicar "Use" + botón (brief §18)
    std::string actionFocusId;  // "use" (solo en focusOrder si actionEnabled)
};

struct CollectionLayout {
    UiRect viewport;

    // Cabecera compartida con el Shop: "Nimvlets" + balance + pestañas
    // "Collection · Shop" (Block 07). Reemplaza a los viejos anclas de
    // título/subtítulo de sección de Block 06.
    SectionHeaderLayout header;

    UiRect dividerRect;            // hairline entre el hero y la gallery (w/h == 0 si la gallery está vacía)
    UiRect galleryShelf;          // segundo plano: fondo un pelín más profundo bajo el divisor (§12; vacío si no hay gallery)

    CollectionHero hero;
    std::vector<GalleryItem> gallery;

    // Block 09C / DEC-136: el owner tiene UN solo Nimvlet -> la gallery
    // queda vacía. En vez de un divisor con un vacío debajo, una línea
    // quieta que apunta al Shop. "" si la gallery tiene entradas.
    std::string emptyGalleryText;
    UiRect emptyGalleryAnchor;    // ancla centrada

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

    // petId de la entrada de gallery bajo el mouse — micro-lift de 2pt
    // instantáneo. "" => ninguna.
    std::string hoverPetId;
};

// Construye el layout hero + gallery completo. Puro y determinista:
// mismas entradas -> mismo resultado, sin SDL, sin medición de texto
// real. TODO el texto traducible ya viene localizado según
// `in.language`.
CollectionLayout BuildCollectionLayout(const catalog::CollectionModel& model, const CollectionLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)].
float ClampScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
