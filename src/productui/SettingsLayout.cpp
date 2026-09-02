#include "productui/SettingsLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "core/DisplayControls.h"
#include "core/Localization.h"
#include "productui/Format.h"

namespace nimvlets::productui {

using core::ClickCountingMode;
using core::Language;
using core::Localized;
using core::PetSizeChoice;
using core::PreferenceField;
using core::StringKey;
using platform::GlobalClickStatusLine;
using platform::GlobalClickUiState;

namespace {

// Métricas en PUNTOS lógicos. Composición compacta: label a la
// izquierda, control segmentado a la derecha; grupos separados por aire
// y una regla sutil (brief §10).
constexpr float kMargin = 40.0f;

constexpr float kGroupTitleH = 15.0f;
constexpr float kGroupTitleToRule = 8.0f;
constexpr float kRuleToRows = 16.0f;
constexpr float kGroupGap = 30.0f;   // fin de un grupo -> título del siguiente

constexpr float kRowH = 30.0f;
constexpr float kRowGap = 14.0f;
constexpr float kLabelColW = 132.0f;   // columna del texto de la fila
constexpr float kLabelToControl = 20.0f;

constexpr float kSegH = 26.0f;
constexpr float kSegGap = 8.0f;
constexpr float kSegPadX = 14.0f;

constexpr float kHintGap = 8.0f;
constexpr float kHintH = 15.0f;

// --- Bloque de aviso del conteo global (Block 11A) ------------------
constexpr float kNoticeGap = 10.0f;      // hint/fila -> etiqueta de estado
constexpr float kNoticeStatusH = 15.0f;
constexpr float kNoticeBodyGap = 6.0f;
constexpr float kNoticeLineH = 15.0f;
constexpr float kNoticeButtonsGap = 10.0f;
constexpr float kNoticeButtonH = 24.0f;
constexpr float kNoticeButtonGap = 8.0f;
constexpr float kNoticeButtonPadX = 14.0f;
// 6 desde Block 11A: la explicación previa al permiso ganó la frase que
// anticipa la redacción AMPLIA del OS, y un párrafo de aviso que se
// corta con "…" justo donde dice qué NO observamos sería peor que uno
// largo. Solo lo alcanzan los cuerpos largos; los demás siguen en 1-2.
constexpr int kNoticeMaxLines = 6;

// Ancho aproximado por carácter — la vista mide fino y centra el texto;
// alcanza para dimensionar el pill y el hit-test (mismo patrón que
// CollectionLayout / ShopLayout).
constexpr float kApproxCharW = 8.0f;

float SegWidth(const std::string& label) {
    return kSegPadX * 2.0f + static_cast<float>(label.size()) * kApproxCharW;
}

// Líneas que ocuparía `body` envuelto en `wrapW`. Estimación pura, con
// el mismo kApproxCharW que dimensiona los pills: esta capa no mide
// texto real (la vista sí, al dibujar con DrawTextWrapped). Acotada a
// kNoticeMaxLines, igual que el dibujo.
int EstimateWrappedLines(const std::string& body, float wrapW) {
    if (body.empty() || wrapW <= 0.0f) {
        return 0;
    }
    const float perLine = std::max(1.0f, wrapW / kApproxCharW);
    const int lines =
        static_cast<int>(std::ceil(static_cast<float>(body.size()) / perLine));
    return std::clamp(lines, 1, kNoticeMaxLines);
}

const char* FieldToken(PreferenceField field) {
    switch (field) {
        case PreferenceField::kSize:
            return "size";
        case PreferenceField::kOpacity:
            return "opacity";
        case PreferenceField::kLockPosition:
            return "lock";
        case PreferenceField::kLanguage:
            return "language";
        case PreferenceField::kClickCounting:
            return "clickcounting";
    }
    return "";
}

struct SegSpec {
    std::string label;
    // Token del focusId. Normalmente "small" / "70" / "on" / "es", que se
    // expande a "opt:<field>:<value>". Si ya viene con el prefijo "cmd:"
    // (filas transitorias de Block 11B: "cmd:show" / "cmd:hide" /
    // "cmd:resetpos") se usa TAL CUAL.
    std::string value;
    bool selected = false;
    bool enabled = true;
};

std::vector<SegSpec> SizeSegs(PetSizeChoice current, Language lang) {
    return {
        {Localized(StringKey::kSizeSmall, lang), "small", current == PetSizeChoice::kSmall},
        {Localized(StringKey::kSizeMedium, lang), "medium", current == PetSizeChoice::kMedium},
        {Localized(StringKey::kSizeLarge, lang), "large", current == PetSizeChoice::kLarge},
    };
}

std::vector<SegSpec> OpacitySegs(int currentPercent) {
    std::vector<SegSpec> out;
    for (const int pct : core::kOpacityChoicesPercent) {
        // Los porcentajes son numéricos: NO se traducen (misma
        // convención que el submenú Opacity del menú rápido).
        out.push_back({std::to_string(pct) + "%", std::to_string(pct), pct == currentPercent});
    }
    return out;
}

std::vector<SegSpec> LockSegs(bool locked, Language lang) {
    return {
        {Localized(StringKey::kOn, lang), "on", locked},
        {Localized(StringKey::kOff, lang), "off", !locked},
    };
}

// [ Nimvlet only ] [ Anywhere ] (Block 11A). "Anywhere" queda apagado
// —dibujado pero no elegible— donde la plataforma no tiene capacidad.
std::vector<SegSpec> ClickCountingSegs(
    ClickCountingMode current, bool anywhereSelectable, Language lang) {
    return {
        {Localized(StringKey::kClickCountingNimvletOnly, lang), "nimvlet_only",
         current == ClickCountingMode::kNimvletOnly, true},
        {Localized(StringKey::kClickCountingAnywhere, lang), "anywhere",
         current == ClickCountingMode::kAnywhere, anywhereSelectable},
    };
}

std::vector<SegSpec> LanguageSegs(Language current) {
    // Endónimos: SIEMPRE en su propio idioma (igual que el submenú
    // Language del menú rápido).
    return {
        {core::LanguageEndonym(Language::kEn), "en", current == Language::kEn},
        {core::LanguageEndonym(Language::kEs), "es", current == Language::kEs},
    };
}

// [ Shown ] [ Hidden ] — Block 11B. TRANSITORIO: el segmento marcado
// sigue el estado de runtime (petShown), no una preferencia persistida.
// focusIds "cmd:show" / "cmd:hide" — se rutean por ParseSettingsCommand,
// no por ParseField.
std::vector<SegSpec> VisibilitySegs(bool petShown, Language lang) {
    return {
        {Localized(StringKey::kVisibilityShown, lang), "cmd:show", petShown},
        {Localized(StringKey::kVisibilityHidden, lang), "cmd:hide", !petShown},
    };
}

// [ Reset position ] — Block 11B. Una ACCIÓN, no un toggle: `selected`
// siempre false. `enabled` sigue la capacidad del backend: en Wayland se
// dibuja apagado y queda fuera del hit-test / foco (brief §9).
std::vector<SegSpec> ResetPositionSegs(bool available, Language lang) {
    return {
        {Localized(StringKey::kResetPosition, lang), "cmd:resetpos", false, available},
    };
}

// Coloca una fila (label + segmentos [+ hint]) a partir de `y`. Devuelve
// la y tras la fila (hint incluido).
float LayoutRow(
    SettingsRow& row, float y, float contentX, float contentW, const std::vector<SegSpec>& segs) {
    row.labelAnchor = UiRect{contentX, y, kLabelColW, kRowH};

    float segX = contentX + kLabelColW + kLabelToControl;
    const float segY = y + (kRowH - kSegH) * 0.5f;
    float focusLeft = segX;
    for (const SegSpec& s : segs) {
        SettingsSegment seg;
        seg.label = s.label;
        seg.focusId = s.value.rfind("cmd:", 0) == 0
                          ? s.value
                          : std::string("opt:") + FieldToken(row.field) + ":" + s.value;
        seg.selected = s.selected;
        seg.enabled = s.enabled;
        const float w = SegWidth(s.label);
        seg.rect = UiRect{segX, segY, w, kSegH};
        row.segments.push_back(seg);
        segX += w + kSegGap;
    }
    const float focusRight = row.segments.empty() ? focusLeft : row.segments.back().rect.Right();
    row.focusRect = UiRect{focusLeft - 4.0f, segY - 4.0f, (focusRight - focusLeft) + 8.0f, kSegH + 8.0f};

    float afterY = y + kRowH;
    if (!row.hint.empty()) {
        row.hintAnchor =
            UiRect{contentX + kLabelColW + kLabelToControl, afterY + kHintGap,
                   contentW - kLabelColW - kLabelToControl, kHintH};
        afterY += kHintGap + kHintH;
    }
    return afterY;
}

// Construye el bloque de aviso de la fila "Click counting" y lo coloca a
// partir de `y`. Devuelve la y tras el bloque. Vacío (present == false)
// cuando no hay nada que decir — el caso normal en modo local sobre una
// plataforma con soporte, que es el estado por defecto del producto.
float LayoutClickCountingNotice(
    SettingsRow& row, float y, float contentX, float contentW, const GlobalClickUiState& gc,
    bool explanationVisible, Language lang) {
    const float bodyX = contentX + kLabelColW + kLabelToControl;
    const float wrapW = std::max(120.0f, contentW - kLabelColW - kLabelToControl);

    SettingsNotice notice;

    if (explanationVisible) {
        // La explicación de PRIMERA PARTE, antes de cualquier pedido de
        // permiso (brief §8). Sin línea de estado: acá lo único que
        // importa es qué se va a pedir y qué NO se observa nunca.
        notice.present = true;
        notice.body = FormatWithPermission(StringKey::kGlobalClickExplain, gc.permissionName, lang);
        notice.buttons.push_back(
            SettingsNoticeButton{Localized(StringKey::kGlobalClickNotNow, lang), UiRect{}, "gc:notnow"});
        notice.buttons.push_back(
            SettingsNoticeButton{Localized(StringKey::kGlobalClickContinue, lang), UiRect{}, "gc:continue"});
    } else {
        switch (gc.statusLine) {
            case GlobalClickStatusLine::kNone:
                break;
            case GlobalClickStatusLine::kActive:
                notice.present = true;
                notice.statusLabel = Localized(StringKey::kGlobalClickActive, lang);
                // Mientras cuenta de verdad: primero el alcance —el
                // owner tiene la entrada del permiso viva en Ajustes del
                // Sistema, con la redacción amplia del OS— y después la
                // semántica de drag, que solo importa acá (brief §21).
                notice.body = std::string(Localized(StringKey::kGlobalClickMouseOnly, lang)) + " " +
                              Localized(StringKey::kGlobalClickDragNote, lang);
                break;
            case GlobalClickStatusLine::kPermissionRequired:
                notice.present = true;
                notice.statusLabel =
                    FormatWithPermission(StringKey::kGlobalClickPermissionNeeded, gc.permissionName, lang);
                notice.statusIsAlert = true;
                // Este es el momento EXACTO en que el owner va a leer
                // la redacción del sistema (el diálogo de TCC ya pasó y
                // está por abrir Ajustes), así que el recordatorio de
                // alcance viaja con el hint.
                notice.body =
                    FormatWithPermission(StringKey::kGlobalClickGrantHint, gc.permissionName, lang) +
                    " " + Localized(StringKey::kGlobalClickMouseOnly, lang);
                break;
            case GlobalClickStatusLine::kUnavailable:
                notice.present = true;
                notice.statusLabel = Localized(StringKey::kGlobalClickUnavailable, lang);
                notice.statusIsAlert = true;
                break;
            case GlobalClickStatusLine::kFailed:
                notice.present = true;
                notice.statusLabel = Localized(StringKey::kGlobalClickFailed, lang);
                notice.statusIsAlert = true;
                break;
        }
        if (notice.present && gc.showCheckAgain) {
            notice.buttons.push_back(SettingsNoticeButton{
                Localized(StringKey::kGlobalClickCheckAgain, lang), UiRect{}, "gc:recheck"});
        }
    }

    if (!notice.present) {
        row.notice = std::move(notice);
        return y;
    }

    float ny = y + kNoticeGap;
    if (!notice.statusLabel.empty()) {
        notice.statusAnchor = UiRect{bodyX, ny, wrapW, kNoticeStatusH};
        ny += kNoticeStatusH;
    }
    if (!notice.body.empty()) {
        notice.bodyLines = EstimateWrappedLines(notice.body, wrapW);
        const float bodyH = static_cast<float>(notice.bodyLines) * kNoticeLineH;
        notice.bodyAnchor = UiRect{bodyX, ny + kNoticeBodyGap, wrapW, bodyH};
        ny += kNoticeBodyGap + bodyH;
    }
    if (!notice.buttons.empty()) {
        ny += kNoticeButtonsGap;
        float bx = bodyX;
        for (SettingsNoticeButton& b : notice.buttons) {
            const float w = kNoticeButtonPadX * 2.0f + static_cast<float>(b.label.size()) * kApproxCharW;
            b.rect = UiRect{bx, ny, w, kNoticeButtonH};
            bx += w + kNoticeButtonGap;
        }
        ny += kNoticeButtonH;
    }

    row.notice = std::move(notice);
    return ny;
}

}  // namespace

GlobalClickAction ParseGlobalClickAction(const std::string& focusId) {
    if (focusId == "gc:continue") {
        return GlobalClickAction::kContinue;
    }
    if (focusId == "gc:notnow") {
        return GlobalClickAction::kNotNow;
    }
    if (focusId == "gc:recheck") {
        return GlobalClickAction::kCheckAgain;
    }
    return GlobalClickAction::kNone;
}

SettingsCommand ParseSettingsCommand(const std::string& focusId) {
    if (focusId == "cmd:show") {
        return SettingsCommand::kShowPet;
    }
    if (focusId == "cmd:hide") {
        return SettingsCommand::kHidePet;
    }
    if (focusId == "cmd:resetpos") {
        return SettingsCommand::kResetPosition;
    }
    return SettingsCommand::kNone;
}

const SettingsRow* SettingsLayout::FindRow(PreferenceField field) const {
    for (const SettingsGroup& g : groups) {
        for (const SettingsRow& r : g.rows) {
            if (r.kind == SettingsRowKind::kPreference && r.field == field) {
                return &r;
            }
        }
    }
    return nullptr;
}

const SettingsRow* SettingsLayout::FindRowKind(SettingsRowKind kind) const {
    for (const SettingsGroup& g : groups) {
        for (const SettingsRow& r : g.rows) {
            if (r.kind == kind) {
                return &r;
            }
        }
    }
    return nullptr;
}

std::string SettingsLayout::HitTest(float x, float y) const {
    for (const SectionTab& tab : header.tabs) {
        if (tab.hitRect.Contains(x, y)) {
            return tab.focusId;
        }
    }
    for (const SettingsGroup& g : groups) {
        for (const SettingsRow& r : g.rows) {
            for (const SettingsSegment& s : r.segments) {
                // Un segmento apagado ("Anywhere" sin capacidad de
                // plataforma) se dibuja pero NUNCA es accionable.
                if (s.enabled && s.rect.Contains(x, y)) {
                    return s.focusId;
                }
            }
            for (const SettingsNoticeButton& b : r.notice.buttons) {
                if (b.rect.Contains(x, y)) {
                    return b.focusId;
                }
            }
        }
    }
    return "";
}

float ClampSettingsScroll(float scrollY, float contentHeight, float viewportH) {
    return std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - viewportH));
}

SettingsLayout BuildSettingsLayout(const SettingsLayoutInput& in) {
    const Language lang = in.prefs.language;
    const float sy = in.scrollY;

    SettingsLayout out;
    out.viewport = UiRect{0.0f, 0.0f, in.viewportW, in.viewportH};
    const float contentW = std::max(240.0f, in.viewportW - 2.0f * kMargin);
    const float contentX = kMargin;

    out.header = BuildSectionHeaderLayout(
        in.viewportW, kMargin, sy, ProductSection::kSettings, lang, in.clickBalance);
    for (const SectionTab& tab : out.header.tabs) {
        out.focusOrder.push_back(tab.focusId);
    }

    float y = out.header.bodyTop;

    // --- Grupo "Companion": visibilidad, tamaño, opacidad, lock, posición ---
    {
        SettingsGroup g;
        g.title = Localized(StringKey::kSettingsCompanion, lang);
        g.titleAnchor = UiRect{contentX, y, contentW, kGroupTitleH};
        y += kGroupTitleH + kGroupTitleToRule;
        g.rule = UiRect{contentX, y, contentW, 1.0f};
        y += 1.0f + kRuleToRows;

        // Visibility (Block 11B): primero — es lo más básico del pet
        // ("¿está en el escritorio?"). TRANSITORIO: el segmento marcado
        // sigue el runtime, no una preferencia.
        SettingsRow visibility;
        visibility.kind = SettingsRowKind::kVisibility;
        visibility.label = Localized(StringKey::kVisibility, lang);
        visibility.focusId = "row:visibility";
        y = LayoutRow(visibility, y, contentX, contentW, VisibilitySegs(in.petShown, lang));
        y += kRowGap;
        g.rows.push_back(std::move(visibility));

        SettingsRow size;
        size.field = PreferenceField::kSize;
        size.label = Localized(StringKey::kSize, lang);
        size.focusId = "row:size";
        y = LayoutRow(size, y, contentX, contentW, SizeSegs(in.prefs.size, lang));
        y += kRowGap;
        g.rows.push_back(std::move(size));

        SettingsRow opacity;
        opacity.field = PreferenceField::kOpacity;
        opacity.label = Localized(StringKey::kOpacity, lang);
        opacity.focusId = "row:opacity";
        y = LayoutRow(opacity, y, contentX, contentW, OpacitySegs(in.prefs.opacityPercent));
        y += kRowGap;
        g.rows.push_back(std::move(opacity));

        SettingsRow lock;
        lock.field = PreferenceField::kLockPosition;
        lock.label = Localized(StringKey::kLockPosition, lang);
        lock.focusId = "row:lock";
        lock.hint = Localized(StringKey::kLockPositionHint, lang);
        y = LayoutRow(lock, y, contentX, contentW, LockSegs(in.prefs.lockPosition, lang));
        y += kRowGap;
        g.rows.push_back(std::move(lock));

        // Position (Block 11B): una acción de recuperación. En Wayland el
        // botón se dibuja apagado y una línea corta lo explica (brief §9);
        // Lock Position NO lo bloquea — es un reset EXPLÍCITO del owner
        // (brief §7). No persiste una preferencia: mueve el pet ahora.
        SettingsRow position;
        position.kind = SettingsRowKind::kPosition;
        position.label = Localized(StringKey::kPosition, lang);
        position.focusId = "row:position";
        if (!in.positionResetAvailable) {
            position.hint = Localized(StringKey::kPositionUnavailable, lang);
        }
        y = LayoutRow(position, y, contentX, contentW,
                      ResetPositionSegs(in.positionResetAvailable, lang));
        g.rows.push_back(std::move(position));

        out.groups.push_back(std::move(g));
    }

    y += kGroupGap;

    // --- Grupo "Interaction": el modo de conteo de clics (Block 11A) ---
    //
    // La PRIMERA preferencia que vive solo en Settings: el menú rápido
    // NO la gana (brief §10). Va entre "Companion" (el pet) y "Language"
    // (chrome de la app), que es donde encaja: es cómo se interactúa con
    // el pet. Las filas de "Companion" no se mueven.
    {
        SettingsGroup g;
        g.title = Localized(StringKey::kSettingsInteraction, lang);
        g.titleAnchor = UiRect{contentX, y, contentW, kGroupTitleH};
        y += kGroupTitleH + kGroupTitleToRule;
        g.rule = UiRect{contentX, y, contentW, 1.0f};
        y += 1.0f + kRuleToRows;

        SettingsRow clicks;
        clicks.field = PreferenceField::kClickCounting;
        clicks.label = Localized(StringKey::kClickCounting, lang);
        clicks.focusId = "row:clickcounting";
        clicks.hint = Localized(StringKey::kClickCountingHint, lang);
        y = LayoutRow(clicks, y, contentX, contentW,
                      ClickCountingSegs(in.prefs.clickCounting, in.globalClick.anywhereSelectable, lang));
        y = LayoutClickCountingNotice(clicks, y, contentX, contentW, in.globalClick,
                                      in.globalClickExplanationVisible, lang);
        g.rows.push_back(std::move(clicks));

        out.groups.push_back(std::move(g));
    }

    y += kGroupGap;

    // --- Grupo "Language": el selector de idioma ---
    {
        SettingsGroup g;
        g.title = Localized(StringKey::kLanguage, lang);
        g.titleAnchor = UiRect{contentX, y, contentW, kGroupTitleH};
        y += kGroupTitleH + kGroupTitleToRule;
        g.rule = UiRect{contentX, y, contentW, 1.0f};
        y += 1.0f + kRuleToRows;

        SettingsRow language;
        language.field = PreferenceField::kLanguage;
        // El encabezado del grupo ("Language" / "Idioma") ya nombra esta
        // fila — no se repite como label de fila. El control arranca en
        // la MISMA columna que los de "Companion" para que las dos
        // secciones se alineen.
        language.label = "";
        language.focusId = "row:language";
        y = LayoutRow(language, y, contentX, contentW, LanguageSegs(in.prefs.language));
        g.rows.push_back(std::move(language));

        out.groups.push_back(std::move(g));
    }

    for (const SettingsGroup& g : out.groups) {
        for (const SettingsRow& r : g.rows) {
            // "Reset position" apagado (Wayland): fuera del anillo de
            // foco, igual que el segmento "Anywhere" sin capacidad — una
            // acción no accionable no debe recibir foco de teclado
            // (brief §9/§20).
            const bool disabledAction =
                r.kind == SettingsRowKind::kPosition &&
                (r.segments.empty() || !r.segments.front().enabled);
            if (!disabledAction) {
                out.focusOrder.push_back(r.focusId);
            }
            // Los botones del aviso se tabulan JUSTO DESPUÉS de su fila:
            // el orden de foco sigue el orden visual, sin saltos.
            for (const SettingsNoticeButton& b : r.notice.buttons) {
                out.focusOrder.push_back(b.focusId);
            }
        }
    }

    out.contentHeight = y + kMargin + sy;
    return out;
}

}  // namespace nimvlets::productui
