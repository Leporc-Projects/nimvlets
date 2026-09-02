#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Preferences.h"
#include "platform/GlobalClickTypes.h"
#include "productui/SectionNav.h"
#include "productui/UiGeometry.h"

namespace nimvlets::productui {

// El layout PURO de la sección Settings (Block 08 + 11A): convierte
// core::Preferences (tamaño / opacidad / lock / idioma — las mismas que
// el menú rápido expone — más el modo de conteo de clics, que solo vive
// acá) + el estado GENÉRICO del monitor de clics globales + tamaño de
// viewport + scroll en un puñado de grupos con controles segmentados, el
// orden de tabulación, y un hit-test por punto. Métricas en PUNTOS
// lógicos. Determinista: mismas entradas -> mismo resultado, sin SDL,
// sin medición de texto real.
//
// **Sin ramas por plataforma** (brief §18). El grupo "Interaction" se
// dibuja a partir de platform::GlobalClickUiState — capacidad, permiso,
// actividad ya resueltos a estado genérico — nunca a partir de un
// `#ifdef __APPLE__` / `#ifdef _WIN32`. El nombre del permiso del OS
// llega como DATO (`GlobalClickUiState::permissionName`).
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
    // "opt:lock:on", "opt:language:es", "opt:clickcounting:anywhere".
    // Solo para hit-test de mouse: el FOCO de teclado vive en
    // "row:<field>".
    std::string focusId;
    bool selected = false;  // = es el valor actual de la preferencia
    // false = la opción existe pero NO se puede elegir en este sistema
    // (Block 11A: "Anywhere" donde la plataforma no soporta el monitor
    // global). Se SIGUE dibujando, apagada, para que la línea de estado
    // ("Not available on this system") tenga a qué referirse — pero
    // queda fuera del HitTest y del recorrido con flechas.
    bool enabled = true;
};

// Una acción del bloque de aviso del conteo global. NO es un cambio de
// preferencia: son los botones del flujo de permiso (brief §8/§9).
enum class GlobalClickAction {
    kNone,
    kContinue,    // ÚNICO camino que puede pedir el permiso nativo
    kNotNow,      // cierra la explicación sin pedir nada
    kCheckAgain,  // re-consulta el permiso e intenta arrancar de nuevo
};

// Traduce un focusId de aviso ("gc:continue" ...). Devuelve kNone si no
// es uno.
GlobalClickAction ParseGlobalClickAction(const std::string& focusId);

// --- Controles TRANSITORIOS de Companion (Block 11B) ---------------
//
// Visibilidad (Shown/Hidden) y "Reset position" NO son preferencias
// persistidas: no van a core::Preferences, no bumpean el schema de
// AppState (brief §4). Por eso NO son un core::PreferenceField ni un
// SettingsChange — son un canal aparte, hermano de GlobalClickAction:
// SettingsView emite un SettingsCommand y src/app lo enruta a la ruta
// canónica correspondiente (SpikeApp::ApplyPetVisibility /
// ResetPetPositionToSafeDefault). Ver docs/PRODUCT_UI.md §20.
enum class SettingsCommand {
    kNone,
    kShowPet,        // fila Visibility -> "Shown"
    kHidePet,        // fila Visibility -> "Hidden"
    kResetPosition,  // fila Position  -> "Reset position"
};

// Traduce un focusId de comando ("cmd:show" / "cmd:hide" /
// "cmd:resetpos"). Devuelve kNone si no es uno.
SettingsCommand ParseSettingsCommand(const std::string& focusId);

// De qué trata una fila. Las filas de Companion/Interaction/Language que
// mutan una PREFERENCIA persistida son kPreference (y llevan un
// core::PreferenceField real); las dos filas transitorias de Block 11B
// no. FindRow(PreferenceField) solo mira las kPreference.
enum class SettingsRowKind {
    kPreference,
    kVisibility,  // [ Shown ] [ Hidden ] — emite un SettingsCommand
    kPosition,    // [ Reset position ]   — emite kResetPosition (o queda apagada)
};

struct SettingsNoticeButton {
    std::string label;    // ya localizado
    UiRect rect;
    std::string focusId;  // "gc:continue" / "gc:notnow" / "gc:recheck"
};

// Bloque de aviso opcional bajo una fila: una etiqueta de estado corta,
// un párrafo, y hasta dos botones. Es el vehículo de TODO lo que el
// brief §8/§9 pide (explicación previa al permiso, estado, reintento)
// sin convertir Settings en un panel de administración.
struct SettingsNotice {
    bool present = false;
    // "Active" / "Input Monitoring permission needed" / … ("" si no hay).
    std::string statusLabel;
    UiRect statusAnchor;
    // true cuando el estado pide atención (falta permiso / falló / no
    // disponible) — la vista lo tinta distinto de un "Active" tranquilo.
    bool statusIsAlert = false;
    // Párrafo envuelto: la explicación de privacidad, el hint para
    // conceder el permiso, o la nota de semántica de drag ("" si no hay).
    std::string body;
    UiRect bodyAnchor;   // ancla IZQUIERDA; .w = ancho de envoltura, .h = alto reservado
    int bodyLines = 0;   // líneas estimadas (layout puro: sin medir texto real)
    std::vector<SettingsNoticeButton> buttons;
};

struct SettingsRow {
    SettingsRowKind kind = SettingsRowKind::kPreference;
    core::PreferenceField field = core::PreferenceField::kSize;  // solo si kind == kPreference
    std::string label;      // "Size" / "Opacity" / "Lock position" / "Language" (localizado)
    UiRect labelAnchor;     // ancla IZQUIERDA del texto de la fila
    std::vector<SettingsSegment> segments;
    std::string focusId;    // "row:<field>" / "row:visibility" / "row:position" — el punto de foco de teclado
    UiRect focusRect;       // anillo de foco alrededor del grupo de segmentos
    std::string hint;       // frase corta bajo la fila ("" salvo Lock position / Click counting)
    UiRect hintAnchor;      // ancla IZQUIERDA del hint (.w = ancho de envoltura)
    SettingsNotice notice;  // bloque de aviso opcional (solo Click counting hoy)
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

    // Orden de tabulación: pestañas de nav, luego las filas de arriba
    // hacia abajo ("row:size", "row:opacity", "row:lock",
    // "row:clickcounting", "row:language"), con los botones del aviso de
    // una fila insertados JUSTO DESPUÉS de esa fila — así Tab desde
    // "Click counting" cae en "Continue"/"Not now"/"Check again" antes de
    // seguir a Language.
    std::vector<std::string> focusOrder;

    float contentHeight = 0.0f;

    // focusId accionable en (x, y): una pestaña de nav, el segmento
    // exacto ("opt:<field>:<value>" o "cmd:show" / "cmd:hide" /
    // "cmd:resetpos"), o un botón de aviso ("gc:..."). "" si nada. Un
    // segmento con `enabled == false` NUNCA se devuelve.
    std::string HitTest(float x, float y) const;

    // La fila de PREFERENCIA de `field` (kind == kPreference), o nullptr.
    const SettingsRow* FindRow(core::PreferenceField field) const;

    // La primera fila de `kind` (para las transitorias de Block 11B:
    // kVisibility / kPosition), o nullptr.
    const SettingsRow* FindRowKind(SettingsRowKind kind) const;
};

struct SettingsLayoutInput {
    float viewportW = 800.0f;
    float viewportH = 560.0f;
    float scrollY = 0.0f;
    core::Preferences prefs;  // idioma y modo de conteo incluidos
    // Estado runtime de Companion, TRANSITORIO (Block 11B): lo empuja
    // src/app junto con las preferencias. NO se persiste.
    //   petShown              — la ventana del pet está visible ahora
    //                           (== !SpikeApp::petHidden_).
    //   positionResetAvailable — este backend puede colocar una toplevel
    //                            en absoluto (macOS / Windows / X11 sí,
    //                            Wayland no — brief §9). Cuando es false,
    //                            "Reset position" se dibuja apagado y
    //                            queda fuera del hit-test y del foco.
    bool petShown = true;
    bool positionResetAvailable = true;
    // Estado GENÉRICO del monitor de clics globales, ya derivado de la
    // capacidad/permiso por platform::ResolveGlobalClickUiState. Su
    // default (kUnavailable) es el correcto para cualquier host sin
    // soporte, así que los tests que no lo tocan siguen valiendo.
    platform::GlobalClickUiState globalClick;
    // true mientras se muestra la explicación de primera parte, ANTES de
    // que se pida ningún permiso (brief §8). Lo gobierna src/app: la
    // vista nunca lo enciende sola.
    bool globalClickExplanationVisible = false;
    // Balance de clics CANÓNICO (de ProductWindow) para la cabecera
    // compartida — ver SectionHeaderLayout::clicksText. Settings NO tiene
    // wallet propio: consume este mismo valor que Collection / Shop
    // (corrección de QA del owner, Block 10).
    std::uint64_t clickBalance = 0;
};

// Construye el layout de Settings. Puro y determinista. Todo el texto
// traducible ya viene localizado según `in.prefs.language`.
SettingsLayout BuildSettingsLayout(const SettingsLayoutInput& in);

// Acota `scrollY` a [0, max(0, contentHeight - viewportH)] (idéntico a
// productui::ClampScroll — se reexpone para no obligar a la vista a
// incluir CollectionLayout.h).
float ClampSettingsScroll(float scrollY, float contentHeight, float viewportH);

}  // namespace nimvlets::productui
