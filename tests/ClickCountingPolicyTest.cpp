#include "ClickCountingPolicyTest.h"

#include <cstdint>
#include <string>

#include "core/ClickCounting.h"
#include "core/Preferences.h"

// La política PURA de conteo de clics de Block 11A. El test central del
// bloque: prueba que un clic físico sobre el pet NUNCA vale +2, en
// ninguna combinación de modo pedido / monitor activo / fuente.
//
// No hace falta ningún mock: core::CountedClickShouldIncrement es una
// función pura de dos enums, y el "wallet" de estos tests es un uint64
// local que se muta EXACTAMENTE como lo hace SpikeApp::HandleCountedClick
// (ver src/app/SpikeApp.cpp) — la misma técnica de "espejar el call site
// real" que tests/ClickAccountingTest.cpp estableció en Block 03.

using nimvlets::core::ClickCountingMode;
using nimvlets::core::ClickCountingModeId;
using nimvlets::core::ClickSource;
using nimvlets::core::CountedClickShouldIncrement;
using nimvlets::core::EffectiveClickCounting;
using nimvlets::core::ParseClickCountingMode;
using nimvlets::core::ResolveEffectiveClickCounting;

namespace nimvlets::tests {

namespace {

// Espejo exacto de SpikeApp::HandleCountedClick: consulta la política y,
// solo si esa fuente cuenta, incrementa el ÚNICO wallet.
void CountedClick(
    std::uint64_t& balance, ClickCountingMode requested, bool monitorActive, ClickSource source) {
    if (CountedClickShouldIncrement(ResolveEffectiveClickCounting(requested, monitorActive), source)) {
        ++balance;
    }
}

// --- Ids persistidos / parseo ------------------------------------

bool TestModeIdsRoundTripAndUnknownFallsBackToLocal() {
    NIMVLETS_CHECK(std::string(ClickCountingModeId(ClickCountingMode::kNimvletOnly)) == "nimvlet_only");
    NIMVLETS_CHECK(std::string(ClickCountingModeId(ClickCountingMode::kAnywhere)) == "anywhere");

    NIMVLETS_CHECK(ParseClickCountingMode("nimvlet_only") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("anywhere") == ClickCountingMode::kAnywhere);

    // LA invariante de migración/robustez (brief §12): cualquier cosa
    // que no sea el id exacto cae al default PRIVADO. Un save v1..v5 no
    // trae el campo -> "" -> kNimvletOnly.
    NIMVLETS_CHECK(ParseClickCountingMode("") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("Anywhere") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("ANYWHERE") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("global") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("anywhere ") == ClickCountingMode::kNimvletOnly);
    NIMVLETS_CHECK(ParseClickCountingMode("true") == ClickCountingMode::kNimvletOnly);

    // Round-trip por el id, en las dos direcciones.
    for (const ClickCountingMode m : {ClickCountingMode::kNimvletOnly, ClickCountingMode::kAnywhere}) {
        NIMVLETS_CHECK(ParseClickCountingMode(ClickCountingModeId(m)) == m);
    }
    return true;
}

// --- Modo pedido -> modo efectivo ---------------------------------

bool TestEffectiveModeNeedsBothTheRequestAndALiveMonitor() {
    // Local pedido: el monitor jamás cambia el resultado.
    NIMVLETS_CHECK(ResolveEffectiveClickCounting(ClickCountingMode::kNimvletOnly, false) ==
                   EffectiveClickCounting::kLocal);
    NIMVLETS_CHECK(ResolveEffectiveClickCounting(ClickCountingMode::kNimvletOnly, true) ==
                   EffectiveClickCounting::kLocal);

    // "Anywhere" pedido pero el monitor NO corre (permiso denegado /
    // revocado / backend ausente / fallo de arranque) -> LOCAL. Nunca
    // "global roto": los clics sobre el pet siguen contando (brief §5).
    NIMVLETS_CHECK(ResolveEffectiveClickCounting(ClickCountingMode::kAnywhere, false) ==
                   EffectiveClickCounting::kLocal);

    // Las dos condiciones juntas, y solo entonces.
    NIMVLETS_CHECK(ResolveEffectiveClickCounting(ClickCountingMode::kAnywhere, true) ==
                   EffectiveClickCounting::kGlobal);
    return true;
}

// --- La matriz completa de 4 casos --------------------------------

bool TestSourceMatrixIsExhaustiveAndSymmetric() {
    NIMVLETS_CHECK(CountedClickShouldIncrement(EffectiveClickCounting::kLocal, ClickSource::kLocalPet));
    // Defensivo: un evento reenviado que llega DESPUÉS de Stop() (o con
    // el modo ya en local) no se cuela en el wallet.
    NIMVLETS_CHECK(
        !CountedClickShouldIncrement(EffectiveClickCounting::kLocal, ClickSource::kGlobalMonitor));
    // EL punto del bloque: en global efectivo la ruta local NO suma.
    NIMVLETS_CHECK(!CountedClickShouldIncrement(EffectiveClickCounting::kGlobal, ClickSource::kLocalPet));
    NIMVLETS_CHECK(
        CountedClickShouldIncrement(EffectiveClickCounting::kGlobal, ClickSource::kGlobalMonitor));

    // En cualquier modo efectivo cuenta EXACTAMENTE una de las dos
    // fuentes — nunca las dos, nunca ninguna.
    for (const EffectiveClickCounting eff :
         {EffectiveClickCounting::kLocal, EffectiveClickCounting::kGlobal}) {
        const int counting = (CountedClickShouldIncrement(eff, ClickSource::kLocalPet) ? 1 : 0) +
                             (CountedClickShouldIncrement(eff, ClickSource::kGlobalMonitor) ? 1 : 0);
        NIMVLETS_CHECK(counting == 1);
    }
    return true;
}

// --- Los tres escenarios del brief §20 ----------------------------

// Modo LOCAL: el clic sobre el pet cuenta; un evento global (que no
// debería existir, porque el monitor no corre) se ignora.
bool TestLocalModeCountsPetClicksAndIgnoresGlobalEvents() {
    std::uint64_t balance = 0;
    const ClickCountingMode requested = ClickCountingMode::kNimvletOnly;
    const bool monitorActive = false;

    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 1);

    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(balance == 1);  // sin cambio

    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 3);
    return true;
}

// Modo GLOBAL EFECTIVO: el monitor es la ÚNICA fuente de moneda. El clic
// sobre el pet sigue disparando su interacción/animación en el runtime
// (eso no pasa por acá) pero NO suma.
bool TestGlobalModeCountsOnlyForwardedEvents() {
    std::uint64_t balance = 0;
    const ClickCountingMode requested = ClickCountingMode::kAnywhere;
    const bool monitorActive = true;

    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(balance == 1);

    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 1);  // la ruta local NO suma en global

    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(balance == 3);
    return true;
}

// "Anywhere" PEDIDO pero inefectivo (permiso denegado/revocado): los
// clics sobre el pet vuelven a contar, y no llega ningún evento global.
// Nunca se descarta un clic en silencio (brief §5).
bool TestRequestedButIneffectiveFallsBackToLocalCounting() {
    std::uint64_t balance = 0;
    const ClickCountingMode requested = ClickCountingMode::kAnywhere;
    const bool monitorActive = false;

    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 2);

    // Y si por alguna razón llegara un evento reenviado tardío, se
    // ignora: el monitor no está activo.
    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(balance == 2);
    return true;
}

// LA regresión del bloque: UN clic físico sobre el pet, en modo global,
// visto por LAS DOS rutas (el monitor global lo ve porque ocurre en el
// sistema; la ventana lo ve porque ocurre encima de ella) vale
// EXACTAMENTE +1.
bool TestOnePhysicalPetClickNeverCountsTwiceInGlobalMode() {
    std::uint64_t balance = 0;
    const ClickCountingMode requested = ClickCountingMode::kAnywhere;
    const bool monitorActive = true;

    // Un solo clic físico, dos observaciones del mismo hecho.
    CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
    CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 1);

    // Diez clics físicos sobre el pet, cada uno visto por las dos rutas.
    for (int i = 0; i < 10; ++i) {
        CountedClick(balance, requested, monitorActive, ClickSource::kGlobalMonitor);
        CountedClick(balance, requested, monitorActive, ClickSource::kLocalPet);
    }
    NIMVLETS_CHECK(balance == 11);
    return true;
}

// Semántica de DRAG en modo global (brief §21): la presión primaria que
// inicia un arrastre ya se contó como evento global; la ruta local de
// drag nunca suma nada, ni en local ni en global. Un arrastre = +1 en
// global, +0 en local.
bool TestDragSemanticsDifferByModeButNeverDoubleCount() {
    // Local: arrastrar el pet NO cuenta (comportamiento histórico —
    // el clasificador devuelve kDrag y ese camino nunca llamó al wallet).
    std::uint64_t local = 0;
    CountedClick(local, ClickCountingMode::kNimvletOnly, false, ClickSource::kLocalPet);  // un clic real
    NIMVLETS_CHECK(local == 1);
    // ...y el gesto de arrastre simplemente no produce ninguna llamada.
    NIMVLETS_CHECK(local == 1);

    // Global: la MISMA presión que inicia el arrastre sí llegó al
    // monitor, así que vale 1 — una sola vez, aunque después el gesto se
    // vuelva un arrastre largo.
    std::uint64_t global = 0;
    CountedClick(global, ClickCountingMode::kAnywhere, true, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(global == 1);
    // El fin del arrastre en la ruta local no agrega nada.
    CountedClick(global, ClickCountingMode::kAnywhere, true, ClickSource::kLocalPet);
    NIMVLETS_CHECK(global == 1);
    return true;
}

// Cambiar de modo en vivo no pierde ni duplica clics: el balance es un
// contador único y continuo, solo cambia QUIÉN lo alimenta.
bool TestSwitchingModesKeepsOneContinuousWallet() {
    std::uint64_t balance = 0;

    // Local: 2 clics sobre el pet.
    CountedClick(balance, ClickCountingMode::kNimvletOnly, false, ClickSource::kLocalPet);
    CountedClick(balance, ClickCountingMode::kNimvletOnly, false, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 2);

    // El owner pasa a "Anywhere" y el monitor arranca: 3 clics globales.
    for (int i = 0; i < 3; ++i) {
        CountedClick(balance, ClickCountingMode::kAnywhere, true, ClickSource::kGlobalMonitor);
    }
    NIMVLETS_CHECK(balance == 5);

    // Vuelve a "Nimvlet only": los eventos globales dejan de contar de
    // inmediato, los del pet vuelven.
    CountedClick(balance, ClickCountingMode::kNimvletOnly, false, ClickSource::kGlobalMonitor);
    NIMVLETS_CHECK(balance == 5);
    CountedClick(balance, ClickCountingMode::kNimvletOnly, false, ClickSource::kLocalPet);
    NIMVLETS_CHECK(balance == 6);
    return true;
}

// core::Preferences: el modo viaja por la MISMA estructura normalizada
// que tamaño/opacidad/lock/idioma, y su default es local.
bool TestPreferencesCarryClickCountingWithLocalDefault() {
    const core::Preferences def;
    NIMVLETS_CHECK(def.clickCounting == ClickCountingMode::kNimvletOnly);

    const core::Preferences fromEmpty =
        core::PreferencesFromStored("medium", 100, false, "en", "");
    NIMVLETS_CHECK(fromEmpty.clickCounting == ClickCountingMode::kNimvletOnly);

    const core::Preferences fromGarbage =
        core::PreferencesFromStored("medium", 100, false, "en", "yes-please");
    NIMVLETS_CHECK(fromGarbage.clickCounting == ClickCountingMode::kNimvletOnly);

    const core::Preferences fromAnywhere =
        core::PreferencesFromStored("medium", 100, false, "en", "anywhere");
    NIMVLETS_CHECK(fromAnywhere.clickCounting == ClickCountingMode::kAnywhere);

    // El overload sin el nuevo argumento (el que usaba Block 08) sigue
    // compilando y dando el default local — nada viejo se rompe.
    const core::Preferences legacy = core::PreferencesFromStored("large", 70, true, "es");
    NIMVLETS_CHECK(legacy.clickCounting == ClickCountingMode::kNimvletOnly);

    // Y participa de la igualdad: dos Preferences que solo difieren en el
    // modo NO son iguales (si no, Settings no se redibujaría al cambiar).
    core::Preferences a;
    core::Preferences b;
    b.clickCounting = ClickCountingMode::kAnywhere;
    NIMVLETS_CHECK(!(a == b));
    return true;
}

}  // namespace

void RegisterClickCountingPolicyTests(testing::TestRunner& runner) {
    runner.Add("ClickCountingPolicy/ModeIdsRoundTripAndUnknownFallsBackToLocal",
               TestModeIdsRoundTripAndUnknownFallsBackToLocal);
    runner.Add("ClickCountingPolicy/EffectiveModeNeedsBothTheRequestAndALiveMonitor",
               TestEffectiveModeNeedsBothTheRequestAndALiveMonitor);
    runner.Add("ClickCountingPolicy/SourceMatrixIsExhaustiveAndSymmetric",
               TestSourceMatrixIsExhaustiveAndSymmetric);
    runner.Add("ClickCountingPolicy/LocalModeCountsPetClicksAndIgnoresGlobalEvents",
               TestLocalModeCountsPetClicksAndIgnoresGlobalEvents);
    runner.Add("ClickCountingPolicy/GlobalModeCountsOnlyForwardedEvents",
               TestGlobalModeCountsOnlyForwardedEvents);
    runner.Add("ClickCountingPolicy/RequestedButIneffectiveFallsBackToLocalCounting",
               TestRequestedButIneffectiveFallsBackToLocalCounting);
    runner.Add("ClickCountingPolicy/OnePhysicalPetClickNeverCountsTwiceInGlobalMode",
               TestOnePhysicalPetClickNeverCountsTwiceInGlobalMode);
    runner.Add("ClickCountingPolicy/DragSemanticsDifferByModeButNeverDoubleCount",
               TestDragSemanticsDifferByModeButNeverDoubleCount);
    runner.Add("ClickCountingPolicy/SwitchingModesKeepsOneContinuousWallet",
               TestSwitchingModesKeepsOneContinuousWallet);
    runner.Add("ClickCountingPolicy/PreferencesCarryClickCountingWithLocalDefault",
               TestPreferencesCarryClickCountingWithLocalDefault);
}

}  // namespace nimvlets::tests
