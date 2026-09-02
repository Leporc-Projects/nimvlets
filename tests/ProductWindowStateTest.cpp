#include "ProductWindowStateTest.h"

#include <cstdint>

#include "core/ClickCounting.h"
#include "productui/ProductWindowState.h"
#include "productui/SectionNav.h"

// La regresión EXACTA que reportó el owner tras la QA física de Block
// 11A: con el Product UI abierto y **Settings** visible, un clic contado
// sobre el Nimvlet subía el balance real pero la cabecera seguía
// mostrando el número viejo hasta cambiar de sección; los clics globales
// sí parecían refrescar en vivo.
//
// La causa era una asimetría de INVALIDACIÓN, no de conteo ni de
// persistencia: ProductWindow asignaba el balance mostrado y marcaba
// sucias a Collection / Shop (que reciben su modelo en el mismo push),
// pero Settings no recibe ninguno y nada la ensuciaba, así que el frame
// no se volvía a dibujar. En modo global el repintado llegaba de rebote
// (clickear fuera cambia la ventana clave y dispara un EXPOSED), no por
// una ruta de notificación real.
//
// El seam mínimo para fijarlo sin SDL es productui::WalletDisplay: el
// wallet mostrado y su decisión de invalidación, que es exactamente lo
// que ProductWindow::SetClickBalance usa. `ProductUiModel` de abajo
// espeja SpikeApp::HandleCountedClick (misma técnica que
// tests/ClickCountingPolicyTest.cpp y tests/ClickAccountingTest.cpp);
// el repintado REAL de la ventana nativa lo prueba el smoke en vivo
// NIMVLETS_DEV_WALLET_LIVE_SMOKE, no este archivo.

using nimvlets::core::ClickCountingMode;
using nimvlets::core::ClickSource;
using nimvlets::core::CountedClickShouldIncrement;
using nimvlets::core::ResolveEffectiveClickCounting;
using nimvlets::productui::ProductSection;
using nimvlets::productui::ResolveWindowPresentStep;
using nimvlets::productui::WalletDisplay;
using nimvlets::productui::WindowPresentStep;

namespace nimvlets::tests {

namespace {

// Espejo del camino canónico de src/app: AppState + el wallet MOSTRADO
// de ProductWindow + el debounce de persistencia, atados por
// SpikeApp::HandleCountedClick.
struct ProductUiModel {
    // --- Estado del programa ---
    std::uint64_t balance = 0;                                  // appState_.clickBalance
    ClickCountingMode requested = ClickCountingMode::kNimvletOnly;
    bool monitorActive = false;

    // --- Estado del Product UI ---
    bool open = false;
    ProductSection section = ProductSection::kCollection;
    WalletDisplay wallet;
    bool repaintPending = false;   // ProductWindow::pendingExpose_
    bool persistenceDirty = false; // persistenceScheduler_.MarkDirty()

    void OpenAt(ProductSection s) {
        open = true;
        section = s;
        wallet.Set(balance);
        repaintPending = false;  // el primer frame ya se dibujó
    }

    // Espejo exacto de SpikeApp::HandleCountedClick.
    void HandleCountedClick(ClickSource source) {
        if (!CountedClickShouldIncrement(
                ResolveEffectiveClickCounting(requested, monitorActive), source)) {
            return;
        }
        ++balance;
        persistenceDirty = true;  // el MISMO debounce de siempre, sin flush inmediato
        if (open) {
            repaintPending = wallet.Set(balance) || repaintPending;
        }
    }

    // Lo que el owner VE: la sección visible se redibuja con el balance
    // canónico si había algo pendiente.
    std::uint64_t Displayed() {
        if (repaintPending) {
            repaintPending = false;
        }
        return wallet.Value();
    }
};

// --- WalletDisplay: el seam de invalidación --------------------------

bool TestWalletInvalidatesOnlyWhenTheNumberChanges() {
    WalletDisplay w;
    NIMVLETS_CHECK(w.Value() == 0);

    NIMVLETS_CHECK(w.Set(1));           // cambió -> hay que repintar
    NIMVLETS_CHECK(w.Value() == 1);
    NIMVLETS_CHECK(!w.Set(1));          // mismo número -> ni un frame de más
    NIMVLETS_CHECK(w.Value() == 1);

    NIMVLETS_CHECK(w.Set(0));           // también baja (una compra) e invalida
    NIMVLETS_CHECK(w.Value() == 0);
    NIMVLETS_CHECK(!w.Set(0));
    return true;
}

// --- El bug del owner, tal cual ------------------------------------

bool TestLocalClickWithSettingsVisibleRefreshesTheWalletImmediately() {
    ProductUiModel app;
    app.balance = 7;
    app.OpenAt(ProductSection::kSettings);
    NIMVLETS_CHECK(app.Displayed() == 7);
    NIMVLETS_CHECK(!app.repaintPending);

    // Modo EFECTIVO local: un clic sobre el Nimvlet es la moneda.
    app.HandleCountedClick(ClickSource::kLocalPet);

    NIMVLETS_CHECK(app.balance == 8);            // el AppState subió
    NIMVLETS_CHECK(app.persistenceDirty);        // el debounce quedó marcado...
    NIMVLETS_CHECK(app.wallet.Value() == 8);     // ...y el wallet CANÓNICO ya vale 8
    // Y la clave: la sección visible tiene un repintado pendiente SIN
    // que el owner haya cambiado de sección. Ese era el eslabón que
    // faltaba antes de esta corrección.
    NIMVLETS_CHECK(app.repaintPending);
    NIMVLETS_CHECK(app.section == ProductSection::kSettings);
    NIMVLETS_CHECK(app.Displayed() == 8);
    return true;
}

bool TestGlobalClickWithSettingsVisibleUsesTheSamePath() {
    ProductUiModel app;
    app.balance = 7;
    app.requested = ClickCountingMode::kAnywhere;
    app.monitorActive = true;  // modo EFECTIVO global
    app.OpenAt(ProductSection::kSettings);

    app.HandleCountedClick(ClickSource::kGlobalMonitor);

    // Paridad EXACTA con el clic local de arriba: mismo incremento,
    // mismo dirty de persistencia, mismo wallet canónico, mismo
    // repintado inmediato.
    NIMVLETS_CHECK(app.balance == 8);
    NIMVLETS_CHECK(app.persistenceDirty);
    NIMVLETS_CHECK(app.wallet.Value() == 8);
    NIMVLETS_CHECK(app.repaintPending);
    NIMVLETS_CHECK(app.Displayed() == 8);
    return true;
}

bool TestEverySectionRefreshesTheSameWay() {
    // El balance vive en la cabecera COMPARTIDA, así que la
    // invalidación no puede depender de la sección: el bug era
    // justamente que Collection / Shop se salvaban de rebote (su vista
    // recibe un modelo nuevo) y Settings no.
    for (const ProductSection s : {ProductSection::kCollection, ProductSection::kShop,
                                   ProductSection::kSettings}) {
        ProductUiModel app;
        app.balance = 41;
        app.OpenAt(s);
        NIMVLETS_CHECK(!app.repaintPending);

        app.HandleCountedClick(ClickSource::kLocalPet);
        NIMVLETS_CHECK(app.balance == 42);
        NIMVLETS_CHECK(app.repaintPending);
        NIMVLETS_CHECK(app.Displayed() == 42);
    }
    return true;
}

bool TestUncountedClickTouchesNothing() {
    // Modo global efectivo: el clic físico sobre el pet NO es moneda (el
    // monitor global ya vio ese mismo clic). No suma, no marca
    // persistencia, y no fuerza ningún repintado — la invalidación
    // mínima necesaria y ni una más.
    ProductUiModel app;
    app.balance = 5;
    app.requested = ClickCountingMode::kAnywhere;
    app.monitorActive = true;
    app.OpenAt(ProductSection::kSettings);

    app.HandleCountedClick(ClickSource::kLocalPet);
    NIMVLETS_CHECK(app.balance == 5);
    NIMVLETS_CHECK(!app.persistenceDirty);
    NIMVLETS_CHECK(!app.repaintPending);

    // Y al revés: en modo local, un evento global que llegara tarde
    // (después de Stop()) tampoco toca nada.
    ProductUiModel local;
    local.balance = 5;
    local.OpenAt(ProductSection::kSettings);
    local.HandleCountedClick(ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(local.balance == 5);
    NIMVLETS_CHECK(!local.persistenceDirty);
    NIMVLETS_CHECK(!local.repaintPending);
    return true;
}

bool TestClosedProductUiStillCountsAndPersists() {
    // Con la ventana cerrada el conteo y el debounce siguen exactamente
    // igual: lo único que no ocurre es el refresco de UI.
    ProductUiModel app;
    app.HandleCountedClick(ClickSource::kLocalPet);
    NIMVLETS_CHECK(app.balance == 1);
    NIMVLETS_CHECK(app.persistenceDirty);
    NIMVLETS_CHECK(!app.repaintPending);
    NIMVLETS_CHECK(app.wallet.Value() == 0);  // no hay nada que mostrar todavía

    // Y al abrirla, el primer frame ya trae el número real.
    app.OpenAt(ProductSection::kSettings);
    NIMVLETS_CHECK(app.Displayed() == 1);
    return true;
}

bool TestManyClicksProduceOneInvalidationEach() {
    ProductUiModel app;
    app.OpenAt(ProductSection::kSettings);
    int repaints = 0;
    for (int i = 0; i < 25; ++i) {
        app.HandleCountedClick(ClickSource::kLocalPet);
        if (app.repaintPending) {
            ++repaints;
        }
        app.Displayed();  // el loop principal dibuja el frame
    }
    NIMVLETS_CHECK(app.balance == 25);
    NIMVLETS_CHECK(app.wallet.Value() == 25);
    NIMVLETS_CHECK(repaints == 25);  // uno por clic contado, ninguno de más
    return true;
}

// --- Recuperar la ventana desde "Collection…" -----------------------

bool TestPresentStepCoversClosedVisibleAndMinimized() {
    // Cerrada: no hay nada que presentar (el que llama la CREA).
    NIMVLETS_CHECK(ResolveWindowPresentStep(false, false) == WindowPresentStep::kNone);
    NIMVLETS_CHECK(ResolveWindowPresentStep(false, true) == WindowPresentStep::kNone);

    // Visible: alcanza con subirla — el contrato preexistente de
    // "reabrir una ventana YA ABIERTA sobre todo la trae al frente".
    NIMVLETS_CHECK(ResolveWindowPresentStep(true, false) == WindowPresentStep::kRaise);

    // Minimizada: hay que RESTAURARLA antes de subirla. Este es el bug
    // #2 del owner — con solo Show/Raise la ventana se quedaba en el
    // Dock y "Collection…" no la recuperaba nunca.
    NIMVLETS_CHECK(ResolveWindowPresentStep(true, true) == WindowPresentStep::kRestoreThenRaise);
    return true;
}

bool TestRestoringNeverCreatesASecondWindowOrResetsState() {
    // Modelo del contrato de SpikeApp::OpenProductWindow ->
    // ProductWindow::Open: con la ventana ya existente, ninguna rama
    // crea otra ni vuelve a la Collection.
    struct Window {
        bool exists = true;
        bool minimized = false;
        ProductSection section = ProductSection::kSettings;
        int created = 0;
        int restored = 0;
        int raised = 0;

        void OpenFromQuickMenu() {
            switch (ResolveWindowPresentStep(exists, minimized)) {
                case WindowPresentStep::kNone:
                    exists = true;
                    ++created;
                    section = ProductSection::kCollection;  // una ventana NUEVA sí arranca ahí
                    break;
                case WindowPresentStep::kRestoreThenRaise:
                    minimized = false;
                    ++restored;
                    ++raised;
                    break;
                case WindowPresentStep::kRaise:
                    ++raised;
                    break;
            }
        }
    };

    Window w;
    w.minimized = true;
    w.OpenFromQuickMenu();
    NIMVLETS_CHECK(w.created == 0);                            // NO hay una segunda ventana
    NIMVLETS_CHECK(w.restored == 1 && w.raised == 1);
    NIMVLETS_CHECK(!w.minimized);
    NIMVLETS_CHECK(w.section == ProductSection::kSettings);    // la sección sobrevive

    // Repetirlo con la ventana ya visible solo la sube otra vez.
    w.OpenFromQuickMenu();
    NIMVLETS_CHECK(w.created == 0 && w.restored == 1 && w.raised == 2);
    NIMVLETS_CHECK(w.section == ProductSection::kSettings);
    return true;
}

}  // namespace

void RegisterProductWindowStateTests(testing::TestRunner& runner) {
    runner.Add("ProductWindowState/WalletInvalidatesOnlyWhenTheNumberChanges",
               TestWalletInvalidatesOnlyWhenTheNumberChanges);
    runner.Add("ProductWindowState/LocalClickWithSettingsVisibleRefreshesTheWalletImmediately",
               TestLocalClickWithSettingsVisibleRefreshesTheWalletImmediately);
    runner.Add("ProductWindowState/GlobalClickWithSettingsVisibleUsesTheSamePath",
               TestGlobalClickWithSettingsVisibleUsesTheSamePath);
    runner.Add("ProductWindowState/EverySectionRefreshesTheSameWay",
               TestEverySectionRefreshesTheSameWay);
    runner.Add("ProductWindowState/UncountedClickTouchesNothing", TestUncountedClickTouchesNothing);
    runner.Add("ProductWindowState/ClosedProductUiStillCountsAndPersists",
               TestClosedProductUiStillCountsAndPersists);
    runner.Add("ProductWindowState/ManyClicksProduceOneInvalidationEach",
               TestManyClicksProduceOneInvalidationEach);
    runner.Add("ProductWindowState/PresentStepCoversClosedVisibleAndMinimized",
               TestPresentStepCoversClosedVisibleAndMinimized);
    runner.Add("ProductWindowState/RestoringNeverCreatesASecondWindowOrResetsState",
               TestRestoringNeverCreatesASecondWindowOrResetsState);
}

}  // namespace nimvlets::tests
