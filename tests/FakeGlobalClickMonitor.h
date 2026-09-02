#pragma once

#include <string>

#include "platform/GlobalClickMonitor.h"

// Doble PURO de platform::GlobalClickMonitor para tests (Block 11A,
// brief §25). Existe porque probar el camino real exigiría un permiso de
// TCC concedido a mano y una sesión gráfica — imposible en CTest, y
// exactamente el tipo de cosa que AGENTS.md §4 no deja declarar
// verificada sin correrla.
//
// Permite simular, en la costura de la interfaz de plataforma:
// disponible / permiso requerido / concedido / denegado / no disponible,
// arrancado / detenido, un clic primario, varios clics, fallo de
// arranque, y caída en runtime.
//
// **Solo infraestructura de test.** No se compila dentro de ninguna
// librería del producto (vive en tests/), y no existe ningún camino que
// lo sustituya por el adapter real en una build de producción: eso sería
// justamente el "DEV bypass que finge que el OS concedió el permiso" que
// el brief §25 prohíbe.

namespace nimvlets::tests {

class FakeGlobalClickMonitor final : public platform::GlobalClickMonitor {
 public:
    // --- Guiones que un test puede montar --------------------------
    platform::GlobalClickCapability capability = platform::GlobalClickCapability::kSupportedNeedsPermission;
    platform::GlobalClickPermission permission = platform::GlobalClickPermission::kNotGranted;
    std::string permissionName = "Test Permission";
    // Si true, Start() falla aunque el permiso esté concedido (fallo del
    // mecanismo nativo).
    bool failStart = false;
    // Qué pasa a `permission` cuando se llama RequestPermission(): en
    // macOS lo NORMAL es que siga sin concederse (el usuario tiene que ir
    // a Ajustes del Sistema), así que el default reproduce ESE caso.
    bool grantOnRequest = false;

    // --- Observables ------------------------------------------------
    int requestPermissionCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;

    platform::GlobalClickStatus QueryStatus() const override {
        platform::GlobalClickStatus s;
        s.capability = capability;
        s.permission = capability == platform::GlobalClickCapability::kSupportedNeedsPermission
                           ? permission
                           : platform::GlobalClickPermission::kNotRequired;
        s.monitorActive = active_;
        s.startFailed = startFailed_;
        s.permissionName =
            capability == platform::GlobalClickCapability::kSupportedNeedsPermission ? permissionName
                                                                                     : std::string();
        return s;
    }

    bool RequestPermission() override {
        ++requestPermissionCalls;
        if (grantOnRequest) {
            permission = platform::GlobalClickPermission::kGranted;
        }
        return permission == platform::GlobalClickPermission::kGranted;
    }

    bool Start(platform::GlobalPrimaryClickCallback callback, void* userData) override {
        ++startCalls;
        if (active_) {
            return true;  // idempotente, igual que el adapter real
        }
        if (capability == platform::GlobalClickCapability::kUnavailable ||
            (capability == platform::GlobalClickCapability::kSupportedNeedsPermission &&
             permission != platform::GlobalClickPermission::kGranted) ||
            failStart || callback == nullptr) {
            startFailed_ = true;
            return false;
        }
        callback_ = callback;
        userData_ = userData;
        active_ = true;
        startFailed_ = false;
        return true;
    }

    void Stop() override {
        ++stopCalls;
        active_ = false;
        callback_ = nullptr;
        userData_ = nullptr;
    }

    bool IsActive() const override { return active_; }

    // --- Estímulos del test ----------------------------------------

    // Simula UNA presión del botón primario observada por el monitor.
    // No-op si no está activo — igual que el adapter real, cuyo callback
    // ya no puede invocarse después de Stop().
    void EmitPrimaryClick() {
        if (active_ && callback_ != nullptr) {
            callback_(userData_);
        }
    }

    void EmitPrimaryClicks(int n) {
        for (int i = 0; i < n; ++i) {
            EmitPrimaryClick();
        }
    }

    // Simula que el monitor se cayó solo en runtime (p. ej. el OS
    // deshabilitó el mecanismo, o el permiso se revocó en vivo).
    void SimulateRuntimeFailure() {
        active_ = false;
        startFailed_ = true;
        callback_ = nullptr;
        userData_ = nullptr;
    }

 private:
    bool active_ = false;
    bool startFailed_ = false;
    platform::GlobalPrimaryClickCallback callback_ = nullptr;
    void* userData_ = nullptr;
};

}  // namespace nimvlets::tests
