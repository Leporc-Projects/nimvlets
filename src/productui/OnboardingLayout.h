#pragma once

#include <string>
#include <vector>

#include "catalog/OnboardingPolicy.h"
#include "catalog/PetIdentity.h"
#include "core/Localization.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// El layout PURO de la pantalla de onboarding de primer arranque (Block
// 09A). Convierte un catalog::OnboardingOffer + la etapa actual + la
// selección tentativa + tamaño de viewport en widgets posicionados, el
// orden de tabulación, y un hit-test por punto. Métricas en PUNTOS
// lógicos. Determinista: mismas entradas -> mismo resultado, sin SDL,
// sin medición de texto real (mismo patrón que CollectionLayout /
// ShopLayout / SettingsLayout).
//
// Onboarding NO es una sección del Product UI (brief §19): NO hay
// cabecera de navegación `Collection · Shop · Settings`, no se puede
// saltear. Composición cálida / quieta / espaciosa: una fila de
// candidatos, el arte manda cuando haya contenido real ligado (brief
// §20). Sin precios, sin lenguaje de Shop, sin rareza, sin "Paso 1 de
// 3" (brief §18/§20).

// Las tres etapas de la interacción. Una sola pantalla; la etapa cambia
// qué se dibuja (brief §22: una segunda elección contenida para Frin,
// sin un wizard de varias páginas).
enum class OnboardingStage {
    kBrowse,       // los 3 normales (+ Frin si el secreto fue revelado)
    kFrinVariant,  // "Which Frin?" -> Male / Female
    kConfirm,      // "Make <name> your first Nimvlet?" -> Cancel / Choose <name>
};

// Una tarjeta de candidato en la fila de selección.
struct OnboardingCandidate {
    catalog::PetIdentity identity;  // {petId,""} normal, {frin,""} Frin lógico, {frin,"male"} variante
    // Qué preview `.nvprev` dibujar: normalmente `identity.variantId`,
    // pero para la tarjeta LÓGICA de Frin ({frin,""}) es la primera
    // variante ("male"), porque no hay preview para el id vacío.
    std::string previewVariantId;
    std::string displayName;        // nombre propio — nunca traducido
    std::string speciesText;        // etiqueta corta de identidad, "" si no hay editorial

    UiRect cell;   // zona clickeable + wash de hover / anillo de foco
    UiRect art;    // caja del arte (la vista resuelve la preview .nvprev)
    UiRect name;   // ancla del nombre (centrada en x)
    UiRect species_;  // ancla de la especie (centrada en x)

    std::string focusId;  // "cand:<petId>" / "cand:frin" / "var:male" / "var:female"
};

// La confirmación (etapa kConfirm). Copy del brief §23.
struct OnboardingConfirm {
    bool visible = false;
    std::string prompt;   // "Make Artu your first Nimvlet?" / "¿Quieres que Artu sea tu primer Nimvlet?"
    UiRect promptAnchor;  // ancla centrada (puede envolver a 2 líneas)

    UiRect cancelButton;
    std::string cancelLabel;    // "Cancel" / "Cancelar"
    std::string cancelFocusId;  // "onb:cancel"

    UiRect chooseButton;
    std::string chooseLabel;    // "Choose Artu" / "Elegir a Artu"
    std::string chooseFocusId;  // "onb:choose"
};

struct OnboardingLayout {
    UiRect viewport;
    OnboardingStage stage = OnboardingStage::kBrowse;

    std::string heading;   // kOnboardingChooseFirst, o kOnboardingWhichVariant en kFrinVariant
    UiRect headingAnchor;  // ancla centrada

    std::vector<OnboardingCandidate> candidates;  // vacío en kConfirm
    OnboardingConfirm confirm;

    // Orden de tabulación. kBrowse: las tarjetas de izquierda a derecha
    // (Frin, si está, SIEMPRE al final — así el reveal no arrebata el
    // foco, brief §21). kFrinVariant: Male, Female. kConfirm: Cancel,
    // Choose (el foco ARRANCA en Cancel — brief §23).
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;

    // focusId accionable en (x, y), o "" si ninguno.
    std::string HitTest(float x, float y) const;
    const OnboardingCandidate* FindCandidate(const std::string& focusId) const;
};

struct OnboardingLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Language language = core::Language::kEn;

    catalog::OnboardingOffer offer;  // incluye offer.secretRevealed
    OnboardingStage stage = OnboardingStage::kBrowse;

    // En kConfirm: qué identidad concreta se va a confirmar (para el
    // prompt y la etiqueta "Choose <name>"). Para Frin ya es la variante
    // elegida ({frin,"male"}).
    catalog::PetIdentity pendingSelection;
    std::string pendingDisplayName;  // nombre propio del pendingSelection
};

// Construye el layout. Puro y determinista. Todo el texto traducible ya
// viene localizado según `in.language`.
OnboardingLayout BuildOnboardingLayout(const OnboardingLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)].
float ClampOnboardingScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
