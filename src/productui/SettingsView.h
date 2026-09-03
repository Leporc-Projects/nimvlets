#pragma once

#include <cstdint>

#include "core/Localization.h"
#include "core/Preferences.h"
#include "productui/FocusList.h"
#include "productui/SectionNav.h"
#include "productui/SettingsLayout.h"
#include "productui/TextCache.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

// Lo que la vista de Settings le pide a src/app: cambiar UNA preferencia
// al valor indicado. `field` dice cuál campo mirar; los demás campos son
// irrelevantes. src/app la enruta por la MISMA ruta canónica que la
// acción equivalente del menú rápido (SpikeApp::Apply* — ver
// docs/PRODUCT_UI.md §20), y re-empuja las preferencias resultantes a
// esta vista (fuente de verdad única, igual que el Shop re-empuja el
// modelo tras una compra).
struct SettingsChange {
    core::PreferenceField field = core::PreferenceField::kSize;
    core::PetSizeChoice size = core::PetSizeChoice::kMedium;
    int opacityPercent = 100;
    bool lockPosition = false;
    core::Language language = core::Language::kEn;
    // Block 11A. Un `kAnywhere` acá es una PETICIÓN, no un hecho: src/app
    // la evalúa (platform::EvaluateGlobalClickRequest) y puede responder
    // mostrando primero la explicación en vez de aplicarla.
    core::ClickCountingMode clickCounting = core::ClickCountingMode::kNimvletOnly;
};

struct SettingsViewResult {
    bool dirty = false;
    bool requestClose = false;    // Escape -> cerrar la ventana
    bool switchSection = false;   // tocó una pestaña de navegación
    ProductSection targetSection = ProductSection::kCollection;
    bool hasChange = false;
    SettingsChange change;
    // Block 11A: una acción del flujo de permiso (Continue / Not now /
    // Check again). Separada de `change` a propósito — no muta ninguna
    // preferencia por sí misma; src/app decide qué hacer.
    bool hasGlobalClickAction = false;
    GlobalClickAction globalClickAction = GlobalClickAction::kNone;
    // Block 11B: un comando de Companion TRANSITORIO (Shown / Hidden /
    // Reset position). Igual que `hasGlobalClickAction`, NO muta ninguna
    // preferencia persistida: src/app lo enruta a la ruta canónica
    // (SpikeApp::ApplyPetVisibility / ResetPetPositionToSafeDefault).
    bool hasCommand = false;
    SettingsCommand command = SettingsCommand::kNone;
};

// La sección SETTINGS del Product UI (Block 08). Misma arquitectura que
// CollectionView / ShopView: input (mouse / teclado / rueda) sobre un
// layout puro (SettingsLayout) + un anillo de foco (FocusList) + un flag
// `dirty_` (event-driven, sin loop). No usa PetPreviewCache ni acento
// por pet — Settings no carga ningún `.nvpack` (brief §23). Sin ventana
// ni event loop propios — ProductWindow la maneja.
//
// La vista NUNCA muta sus propias preferencias: emite un SettingsChange
// y espera que src/app le re-empuje el estado final vía SetPreferences.
// Así lo que se dibuja siempre refleja el AppState canónico, y el menú
// rápido y Settings no pueden divergir (brief §6/§7).
class SettingsView {
 public:
    // Empuja las preferencias actuales (tamaño / opacidad / lock /
    // idioma). Lo llama src/app al abrir Settings y CADA vez que una
    // preferencia cambia — venga de Settings o del menú rápido (brief
    // §7, sincronización bidireccional).
    void SetPreferences(core::Preferences prefs);

    // Solo el idioma (para el camino de SetLanguage que ProductWindow ya
    // usa en las otras vistas). Equivale a SetPreferences con el mismo
    // resto de campos.
    void SetLanguage(core::Language language);

    // Anchos MEDIDOS de los rótulos de nav (serif), empujados por
    // ProductWindow cuando cambian idioma/escala (convergencia DEC-147).
    // BuildLayout() los pasa a ReflowNavTabs.
    void SetNavLabelWidths(const float w[3]) {
        navLabelWidths_[0] = w[0];
        navLabelWidths_[1] = w[1];
        navLabelWidths_[2] = w[2];
    }

    // Estado GENÉRICO del monitor de clics globales + si la explicación
    // de primera parte está visible (Block 11A). Lo empuja src/app: la
    // vista NUNCA enciende la explicación sola ni deduce el estado del
    // permiso — igual que nunca muta sus propias preferencias.
    void SetGlobalClick(const platform::GlobalClickUiState& state, bool explanationVisible);

    // Estado runtime de Companion TRANSITORIO (Block 11B): si la ventana
    // del pet está visible AHORA, y si este backend puede colocar una
    // toplevel en absoluto ("Reset position" disponible). Lo empuja
    // src/app junto con las preferencias — la vista nunca lo deduce ni lo
    // persiste. Ver docs/PRODUCT_UI.md §20.
    void SetCompanionRuntime(bool petShown, bool positionResetAvailable);

    const core::Preferences& Preferences() const { return prefs_; }

    // Coordenadas en PUNTOS lógicos de la ventana.
    SettingsViewResult OnMouseMove(float x, float y);
    SettingsViewResult OnMouseDown(float x, float y);
    SettingsViewResult OnWheel(float dyLines);
    SettingsViewResult OnKey(int sdlKeycode, bool shiftHeld);
    SettingsViewResult OnViewportChanged();

    bool Dirty() const { return dirty_; }
    void ClearDirty() { dirty_ = false; }

    // Al ENTRAR a la sección: foco en la pestaña "Settings", sin hover,
    // sin chrome de foco de teclado — punto de partida predecible (brief
    // §18).
    void OnEnterSection();

    // Solo-DEV (QA / capturas): pone el foco de TECLADO sobre una fila
    // ("row:visibility" / "row:size" / "row:opacity" / "row:lock" /
    // "row:position" / "row:clickcounting" / "row:language") para la
    // captura del anillo de foco.
    void SetKeyboardFocusForQA(const std::string& rowFocusId) {
        SyncFocusList(BuildLayout(viewportW_, viewportH_));
        focus_.Focus(rowFocusId);
        keyboardFocus_ = true;
        dirty_ = true;
    }

    // `clickBalance` es el balance CANÓNICO de ProductWindow — Settings
    // NO tiene wallet propio (corrección de QA del owner, Block 10). Se
    // dibuja en la MISMA cabecera compartida que Collection / Shop.
    void Render(
        UiPainter& painter, TextCache& text, float viewportW, float viewportH,
        std::uint64_t clickBalance);

 private:
    SettingsLayout BuildLayout(float w, float h) const;
    void SyncFocusList(const SettingsLayout& layout);
    SettingsViewResult ActivateWidget(const std::string& focusId);
    SettingsChange ChangeForSegment(const std::string& focusId) const;
    SettingsChange ChangeForStep(core::PreferenceField field, int dir, bool wrap) const;
    // false para un `kAnywhere` pedido en un sistema sin capacidad — así
    // ni el teclado ni el mouse pueden elegir una opción que la línea de
    // estado ya declara no disponible.
    bool ChangeIsAllowed(const SettingsChange& change) const;
    // Dibuja el bloque de aviso de una fila (estado + párrafo + botones).
    void DrawNotice(
        UiPainter& painter, TextCache& text, const SettingsNotice& notice,
        const std::string& focusedId) const;

    core::Preferences prefs_;
    platform::GlobalClickUiState globalClick_;
    bool globalClickExplanationVisible_ = false;
    // Estado runtime de Companion (Block 11B), empujado por src/app.
    // TRANSITORIO: nunca se persiste.
    bool petShown_ = true;
    bool positionResetAvailable_ = true;
    // Cache del balance CANÓNICO: lo fija Render() cada frame (valor de
    // ProductWindow) para que BuildLayout() lo pase a la cabecera
    // compartida — igual que Collection / Shop.
    std::uint64_t clickBalance_ = 0;
    // Anchos MEDIDOS de los rótulos de nav (convergencia DEC-147).
    float navLabelWidths_[3] = {0.0f, 0.0f, 0.0f};

    FocusList focus_;

    float scrollY_ = 0.0f;
    float lastContentHeight_ = 0.0f;
    float viewportW_ = 800.0f;
    float viewportH_ = 560.0f;

    std::string hoverId_;
    bool keyboardFocus_ = false;
    bool dirty_ = true;
};

}  // namespace nimvlets::productui
