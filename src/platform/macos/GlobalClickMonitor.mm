#include "platform/GlobalClickMonitor.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

// Monitor de clics primarios globales de macOS (Block 11A).
//
// API elegida, tras investigar las alternativas soportadas:
//
//   - `CGPreflightListenEventAccess()` / `CGRequestListenEventAccess()`
//     (CoreGraphics, macOS 10.15+) para el permiso. Categoría de TCC:
//     **Input Monitoring**. NO Accessibility (`AXIsProcessTrusted` no
//     aparece en este archivo ni en ningún otro de src/), NO Screen
//     Recording. Ver docs/PRIVACY_SECURITY.md §H.
//   - `CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
//     kCGEventTapOptionListenOnly, ...)` con la máscara de UN SOLO
//     evento: `CGEventMaskBit(kCGEventLeftMouseDown)`.
//
// **Listen-only, sin excepción.** Nimvlets nunca modifica, suprime,
// retrasa ni consume el clic del usuario: el tap se crea con
// `kCGEventTapOptionListenOnly`, que hace que el valor de retorno del
// callback ni siquiera se consulte, y el callback devuelve el evento tal
// cual de todas formas.
//
// **Qué observa el callback: el TIPO del evento, y nada más.** No lee
// coordenadas (`CGEventGetLocation`), ni el botón, ni el timestamp, ni
// la ventana/app destino (`kCGEventTargetUnixProcessID` &co.), ni
// modificadores. Su cuerpo entero es "si es un left-mouse-down, avisá".
//
// **Por qué un hilo dedicado** (brief §7). El loop principal sí sirve
// `NSDefaultRunLoopMode` mientras espera eventos —
// `Cocoa_PumpEventsUntilDate` en la SDL pineada usa
// `[NSApp nextEventMatchingMask:untilDate:inMode:NSDefaultRunLoopMode]`,
// leído directamente de la fuente (AGENTS.md §4) — pero solo mientras
// ESPERA. Durante un `RenderFrame()`, un `ApplyCurrentHitMask()`, la
// carga de un `.nvpack` al cambiar de pet, o un redibujo del Product UI,
// el run loop principal no atiende sources. Un CGEventTap tiene timeout
// duro: si su callback no responde a tiempo, el sistema DESHABILITA el
// tap (`kCGEventTapDisabledByTimeout`). Colgar el tap del run loop
// principal haría que el conteo global se apagara solo justo cuando el
// owner está interactuando. El hilo dedicado no hace absolutamente nada
// más: bloquea en `CFRunLoopRun()` (event-driven, cero polling, cero
// wakeups periódicos) y solo existe mientras el modo global está activo.
//
// Sin ARC, igual que el resto de src/platform/macos.

namespace nimvlets::platform {

namespace {

// El nombre del permiso TAL COMO lo llama macOS. Lo consume la copy de
// Settings vía GlobalClickStatus::permissionName, para que src/productui
// no tenga ninguna rama por plataforma (brief §18).
constexpr const char* kPermissionName = "Input Monitoring";

class MacGlobalClickMonitor final : public GlobalClickMonitor {
 public:
    ~MacGlobalClickMonitor() override { Stop(); }

    GlobalClickStatus QueryStatus() const override {
        GlobalClickStatus status;
        status.capability = GlobalClickCapability::kSupportedNeedsPermission;
        // Preflight: consulta pura, NUNCA muestra un diálogo. Es lo que
        // permite que el arranque de la app decida si puede encender el
        // monitor sin sorprender al owner con un prompt de TCC (brief §8).
        status.permission = CGPreflightListenEventAccess() ? GlobalClickPermission::kGranted
                                                           : GlobalClickPermission::kNotGranted;
        status.monitorActive = active_.load(std::memory_order_acquire);
        status.startFailed = startFailed_.load(std::memory_order_acquire);
        status.permissionName = kPermissionName;
        return status;
    }

    bool RequestPermission() override {
        // Único punto de todo el repositorio que puede provocar el
        // diálogo de Input Monitoring. src/app la llama SOLO tras el
        // "Continue" del owner sobre la explicación de primera parte.
        //
        // macOS devuelve el estado inmediato, que típicamente sigue
        // siendo false: el diálogo solo ofrece abrir Ajustes del
        // Sistema, y el permiso recién queda efectivo cuando el usuario
        // lo activa ahí. Eso NO es un fallo — Settings lo refleja como
        // "falta permiso" + "Comprobar de nuevo".
        return CGRequestListenEventAccess();
    }

    bool Start(GlobalPrimaryClickCallback callback, void* userData) override {
        if (active_.load(std::memory_order_acquire)) {
            return true;  // idempotente
        }
        if (callback == nullptr) {
            return false;
        }
        if (!CGPreflightListenEventAccess()) {
            // Nunca se intenta crear el tap sin permiso: CGEventTapCreate
            // fallaría igual, y preguntar primero mantiene el estado
            // reportado a Settings coherente.
            startFailed_.store(true, std::memory_order_release);
            return false;
        }

        callback_ = callback;
        userData_ = userData;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            startDone_ = false;
            startOk_ = false;
        }
        thread_ = std::thread([this] { ThreadMain(); });

        std::unique_lock<std::mutex> lock(mutex_);
        // Acotado: si algo del lado nativo se colgara, el hilo principal
        // no se queda esperando para siempre — reporta el fallo y sigue
        // en modo local.
        const bool signalled = startCv_.wait_for(
            lock, std::chrono::seconds(2), [this] { return startDone_; });
        const bool ok = signalled && startOk_;
        lock.unlock();

        if (!ok) {
            StopThread();
            startFailed_.store(true, std::memory_order_release);
            callback_ = nullptr;
            userData_ = nullptr;
            return false;
        }

        startFailed_.store(false, std::memory_order_release);
        active_.store(true, std::memory_order_release);
        return true;
    }

    void Stop() override {
        StopThread();
        active_.store(false, std::memory_order_release);
        // Recién acá el callback deja de poder invocarse: StopThread()
        // hace join, así que el hilo del tap ya terminó (brief §19 — sin
        // callbacks nativos colgando cuando SpikeApp se está apagando).
        callback_ = nullptr;
        userData_ = nullptr;
    }

    bool IsActive() const override { return active_.load(std::memory_order_acquire); }

 private:
    // --- Callback del tap: se ejecuta en el HILO DEDICADO ------------
    //
    // Todo lo que hace es reenviar "hubo un clic primario". Sin formato,
    // sin AppState, sin persistencia, sin coordenadas, sin inspección
    // del destino, sin trabajo caro (brief §7).
    static CGEventRef TapCallback(
        CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
        auto* self = static_cast<MacGlobalClickMonitor*>(refcon);
        if (self == nullptr) {
            return event;
        }

        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            // El sistema puede deshabilitar un tap; re-habilitarlo es la
            // respuesta correcta y documentada. No se cuenta como clic.
            if (self->tap_ != nullptr) {
                CGEventTapEnable(self->tap_, true);
            }
            return event;
        }

        if (type == kCGEventLeftMouseDown) {
            const GlobalPrimaryClickCallback cb = self->callback_;
            if (cb != nullptr) {
                cb(self->userData_);
            }
        }
        // Listen-only: el retorno se ignora. Se devuelve el evento
        // INTACTO igual, para que no exista ni la posibilidad de alterar
        // el clic del usuario.
        return event;
    }

    // Perform de la source de parada. Corre en el hilo dedicado.
    static void StopPerform(void* /*info*/) { CFRunLoopStop(CFRunLoopGetCurrent()); }

    void ThreadMain() {
        // El tap se crea EN el hilo que va a correr el run loop.
        tap_ = CGEventTapCreate(
            kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
            CGEventMaskBit(kCGEventLeftMouseDown), &MacGlobalClickMonitor::TapCallback, this);

        CFRunLoopSourceRef tapSource = nullptr;
        if (tap_ != nullptr) {
            tapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap_, 0);
        }

        if (tap_ == nullptr || tapSource == nullptr) {
            if (tapSource != nullptr) {
                CFRelease(tapSource);
            }
            if (tap_ != nullptr) {
                CFRelease(tap_);
                tap_ = nullptr;
            }
            SignalStartResult(false);
            return;
        }

        CFRunLoopRef runLoop = CFRunLoopGetCurrent();

        // Source de parada, versión 0: se puede SEÑALAR desde otro hilo
        // y el run loop la atiende en cuanto corre. Es lo que elimina la
        // carrera de "Stop() llegó antes de que CFRunLoopRun() empezara"
        // sin recurrir a un wakeup periódico (una señal queda LATCHED).
        CFRunLoopSourceContext stopCtx{};
        stopCtx.perform = &MacGlobalClickMonitor::StopPerform;
        CFRunLoopSourceRef stopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &stopCtx);

        CFRunLoopAddSource(runLoop, tapSource, kCFRunLoopCommonModes);
        if (stopSource != nullptr) {
            CFRunLoopAddSource(runLoop, stopSource, kCFRunLoopCommonModes);
        }
        CGEventTapEnable(tap_, true);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            runLoop_ = runLoop;
            stopSource_ = stopSource;
        }
        SignalStartResult(true);

        // Bloquea hasta que StopPerform corra. Sin timeout, sin
        // wakeups: mientras no haya clics el hilo consume cero CPU.
        CFRunLoopRun();

        CGEventTapEnable(tap_, false);
        CFRunLoopRemoveSource(runLoop, tapSource, kCFRunLoopCommonModes);
        CFRelease(tapSource);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopSource_ != nullptr) {
                CFRunLoopRemoveSource(runLoop, stopSource_, kCFRunLoopCommonModes);
                CFRelease(stopSource_);
                stopSource_ = nullptr;
            }
            runLoop_ = nullptr;
        }
        CFRelease(tap_);
        tap_ = nullptr;
    }

    void SignalStartResult(bool ok) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            startOk_ = ok;
            startDone_ = true;
        }
        startCv_.notify_all();
    }

    // Para y hace join del hilo si existe. Idempotente.
    void StopThread() {
        if (!thread_.joinable()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopSource_ != nullptr) {
                CFRunLoopSourceSignal(stopSource_);
            }
            if (runLoop_ != nullptr) {
                CFRunLoopWakeUp(runLoop_);
            }
        }
        thread_.join();
    }

    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable startCv_;
    bool startDone_ = false;
    bool startOk_ = false;

    // Solo los toca el hilo dedicado (tap_) o se leen bajo mutex_.
    CFMachPortRef tap_ = nullptr;
    CFRunLoopRef runLoop_ = nullptr;
    CFRunLoopSourceRef stopSource_ = nullptr;

    // Se fijan antes de arrancar el hilo y se limpian después del join,
    // así que el hilo del tap nunca los ve cambiar mientras corre.
    GlobalPrimaryClickCallback callback_ = nullptr;
    void* userData_ = nullptr;

    std::atomic<bool> active_{false};
    std::atomic<bool> startFailed_{false};
};

}  // namespace

std::unique_ptr<GlobalClickMonitor> CreateGlobalClickMonitor() {
    return std::make_unique<MacGlobalClickMonitor>();
}

}  // namespace nimvlets::platform
