#include "ButtonStyleTest.h"

#include "productui/ButtonStyle.h"
#include "productui/Contrast.h"
#include "productui/PetAccent.h"
#include "productui/VisualTokens.h"

using nimvlets::productui::AccentEmphasis;
using nimvlets::productui::ButtonRole;
using nimvlets::productui::ButtonStateFlags;
using nimvlets::productui::ButtonVisual;
using nimvlets::productui::ContrastRatio;
using nimvlets::productui::PetAccent;
using nimvlets::productui::PetAccentFor;
using nimvlets::productui::RelativeLuminance;
using nimvlets::productui::ResolveButtonVisual;
namespace tokens = nimvlets::productui::tokens;

namespace nimvlets::tests {

namespace {

ButtonStateFlags Rest() { return ButtonStateFlags{}; }

// Primary con contexto de pet: relleno = softFill del acento, borde =
// line del acento, y la etiqueta es LEGIBLE sobre el relleno (>= 4.5:1)
// para cada pet real y el fallback neutro.
bool TestPrimaryUsesPetAccentAndStaysReadable() {
    for (const char* id : {"bunny", "nidir", "frin", "sweetie", "does_not_exist"}) {
        const PetAccent acc = PetAccentFor(id);
        const ButtonVisual v = ResolveButtonVisual(ButtonRole::kPrimary, &acc, Rest());
        NIMVLETS_CHECK(v.fill == acc.softFill);
        NIMVLETS_CHECK(v.border == acc.line);
        NIMVLETS_CHECK(v.fill.a == 255 && v.ink.a == 255);
        NIMVLETS_CHECK(ContrastRatio(v.ink, v.fill) >= 4.5);
    }
    return true;
}

// Primary sin contexto de pet (accent == nullptr) cae al neutro cálido
// y sigue siendo legible.
bool TestPrimaryNullAccentFallsBackToNeutral() {
    const ButtonVisual v = ResolveButtonVisual(ButtonRole::kPrimary, nullptr, Rest());
    const PetAccent neutral = PetAccentFor("");
    NIMVLETS_CHECK(v.fill == neutral.softFill);
    NIMVLETS_CHECK(ContrastRatio(v.ink, v.fill) >= 4.5);
    return true;
}

// Un acento sintético demasiado claro para su propio deepInk: el
// resolvedor CLAMPA la tinta más oscura en vez de devolver un par
// ilegible (nunca texto invisible — brief §17).
bool TestPrimaryClampsUnreadableAccent() {
    PetAccent bad = PetAccentFor("frin");
    bad.softFill = nimvlets::productui::UiColor{0xF2, 0xF2, 0xF2, 0xFF};
    bad.deepInk = nimvlets::productui::UiColor{0xC8, 0xC8, 0xC8, 0xFF};  // gris claro sobre casi-blanco: ilegible
    const ButtonVisual v = ResolveButtonVisual(ButtonRole::kPrimary, &bad, Rest());
    NIMVLETS_CHECK(ContrastRatio(v.ink, v.fill) >= 4.5);
    NIMVLETS_CHECK(!(v.ink == bad.deepInk));  // se tuvo que oscurecer
    return true;
}

// Secondary / Quiet no tienen relleno en reposo; Quiet además no tiene
// borde.
bool TestSecondaryAndQuietAreRestrainedAtRest() {
    const ButtonVisual sec = ResolveButtonVisual(ButtonRole::kSecondary, nullptr, Rest());
    NIMVLETS_CHECK(sec.fill.a == 0);
    NIMVLETS_CHECK(sec.border == tokens::kBorder);
    NIMVLETS_CHECK(sec.ink == tokens::kTextSecondary);

    const ButtonVisual quiet = ResolveButtonVisual(ButtonRole::kQuiet, nullptr, Rest());
    NIMVLETS_CHECK(quiet.fill.a == 0);
    NIMVLETS_CHECK(quiet.border.a == 0);
    return true;
}

// Estados: foco pide el anillo; hover pinta un wash; disabled queda
// legible-pero-apagado y SIN anillo de foco.
bool TestInteractionStates() {
    const ButtonVisual focused =
        ResolveButtonVisual(ButtonRole::kSecondary, nullptr, ButtonStateFlags{false, false, true, false});
    NIMVLETS_CHECK(focused.drawFocusRing);

    const ButtonVisual hoveredQuiet =
        ResolveButtonVisual(ButtonRole::kQuiet, nullptr, ButtonStateFlags{true, false, false, false});
    NIMVLETS_CHECK(hoveredQuiet.fill == tokens::kHoverWash);

    const PetAccent nidir = PetAccentFor("nidir");
    const ButtonVisual disabled = ResolveButtonVisual(
        ButtonRole::kPrimary, &nidir, ButtonStateFlags{true, false, true, true});
    NIMVLETS_CHECK(!disabled.drawFocusRing);
    NIMVLETS_CHECK(disabled.ink == tokens::kTextMuted);
    return true;
}

// Convergencia DEC-147: kPrimaryCta — la CTA grande. Trae los adornos
// (pill, spark, filo de oro, 2-tono) y una etiqueta LEGIBLE sobre su
// relleno para cada pet real + el neutro. El relleno es distinto (más
// saturado) del Primary contenido.
bool TestPrimaryCtaHasOrnamentsAndReadableInk() {
    for (const char* id : {"bunny", "nidir", "frin", "kyubi", "does_not_exist"}) {
        const PetAccent acc = PetAccentFor(id);
        const ButtonVisual cta = ResolveButtonVisual(ButtonRole::kPrimaryCta, &acc, Rest());
        NIMVLETS_CHECK(cta.pill);
        NIMVLETS_CHECK(cta.sparkle);
        NIMVLETS_CHECK(cta.edgeAccent.a != 0);
        NIMVLETS_CHECK(cta.topHighlight.a != 0 && cta.bottomShade.a != 0);
        NIMVLETS_CHECK(cta.fill.a == 255 && cta.ink.a == 255);
        NIMVLETS_CHECK(ContrastRatio(cta.ink, cta.fill) >= 4.5);
        // Relleno de la CTA != relleno del Primary contenido (más color).
        const ButtonVisual plain = ResolveButtonVisual(ButtonRole::kPrimary, &acc, Rest());
        NIMVLETS_CHECK(!(cta.fill == plain.fill));
        NIMVLETS_CHECK(!plain.pill && !plain.sparkle && plain.edgeAccent.a == 0);
    }
    return true;
}

// DEC-148: la estrategia centralizada de ÉNFASIS del CTA. Un pet "hondo"
// (Nidir) recibe un relleno de acento SATURADO y OSCURO (el CTA violeta
// del concept) con tinta cream; un pet "suave" (Bunny) recibe el relleno
// claro con tinta oscura. Los dos SIEMPRE ≥ 4.5:1 (BestForeground), y el
// relleno hondo es netamente más oscuro que el suave.
bool TestPrimaryCtaEmphasisStrategy() {
    const PetAccent nidir = PetAccentFor("nidir");
    const PetAccent bunny = PetAccentFor("bunny");
    NIMVLETS_CHECK(nidir.emphasis == AccentEmphasis::kDeep);
    NIMVLETS_CHECK(bunny.emphasis == AccentEmphasis::kSoft);

    const ButtonVisual deep = ResolveButtonVisual(ButtonRole::kPrimaryCta, &nidir, Rest());
    const ButtonVisual soft = ResolveButtonVisual(ButtonRole::kPrimaryCta, &bunny, Rest());

    // Los dos: legibles y con el filo de oro + 2-tono + pill + spark.
    for (const ButtonVisual& v : {deep, soft}) {
        NIMVLETS_CHECK(ContrastRatio(v.ink, v.fill) >= 4.5);
        NIMVLETS_CHECK(v.pill && v.sparkle && v.edgeAccent.a != 0);
    }
    // El relleno hondo de Nidir es marcadamente más oscuro que el suave
    // de Bunny, y su tinta es CLARA (cream) — no la tinta oscura del suave.
    NIMVLETS_CHECK(RelativeLuminance(deep.fill) < RelativeLuminance(soft.fill) - 0.15);
    NIMVLETS_CHECK(RelativeLuminance(deep.ink) > RelativeLuminance(deep.fill));   // tinta clara sobre relleno hondo
    NIMVLETS_CHECK(RelativeLuminance(soft.ink) < RelativeLuminance(soft.fill));   // tinta oscura sobre relleno claro
    // El contorno interior del hondo es MÁS OSCURO que su relleno (filo
    // limpio, nunca un anillo pálido — brief §19).
    NIMVLETS_CHECK(RelativeLuminance(deep.border) < RelativeLuminance(deep.fill));
    return true;
}

// kPrimaryCta disabled: sin adornos, legible-pero-apagada, sin anillo.
bool TestPrimaryCtaDisabledDropsOrnaments() {
    const PetAccent bunny = PetAccentFor("bunny");
    const ButtonVisual d = ResolveButtonVisual(
        ButtonRole::kPrimaryCta, &bunny, ButtonStateFlags{true, false, true, true});
    NIMVLETS_CHECK(!d.sparkle);
    NIMVLETS_CHECK(d.edgeAccent.a == 0 && d.topHighlight.a == 0 && d.bottomShade.a == 0);
    NIMVLETS_CHECK(!d.drawFocusRing);
    NIMVLETS_CHECK(d.ink == tokens::kTextMuted);
    return true;
}

}  // namespace

void RegisterButtonStyleTests(testing::TestRunner& runner) {
    runner.Add("ButtonStyle/PrimaryCtaHasOrnamentsAndReadableInk",
               TestPrimaryCtaHasOrnamentsAndReadableInk);
    runner.Add("ButtonStyle/PrimaryCtaEmphasisStrategy", TestPrimaryCtaEmphasisStrategy);
    runner.Add("ButtonStyle/PrimaryCtaDisabledDropsOrnaments", TestPrimaryCtaDisabledDropsOrnaments);
    runner.Add("ButtonStyle/PrimaryUsesPetAccentAndStaysReadable",
               TestPrimaryUsesPetAccentAndStaysReadable);
    runner.Add("ButtonStyle/PrimaryNullAccentFallsBackToNeutral",
               TestPrimaryNullAccentFallsBackToNeutral);
    runner.Add("ButtonStyle/PrimaryClampsUnreadableAccent", TestPrimaryClampsUnreadableAccent);
    runner.Add("ButtonStyle/SecondaryAndQuietAreRestrainedAtRest",
               TestSecondaryAndQuietAreRestrainedAtRest);
    runner.Add("ButtonStyle/InteractionStates", TestInteractionStates);
}

}  // namespace nimvlets::tests
