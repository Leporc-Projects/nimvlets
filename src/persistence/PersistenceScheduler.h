#pragma once

#include <optional>

namespace nimvlets::persistence {

// Scheduler de dirty-flag con debounce para escrituras de AppState —
// lógica de timing pura, sin I/O de archivos, sin SDL, testeable con
// timestamps fabricados exactamente igual que core::FrameScheduler
// (ver tests/PersistenceSchedulerTest.cpp).
//
// El problema que resuelve: los clicks pueden llegar muchas veces por
// segundo, pero escribir a disco una vez por click sería un
// desperdicio sin sentido (ver docs/PERSISTENCE.md, "política de
// escritura"). En cambio, el primer cambio después de un estado
// limpio/flusheado arma un único deadline `debounceMs` en el futuro;
// cualquier cambio posterior antes de que ese deadline dispare se
// coalesce en lo que sea que se escriba en ese único deadline — no
// arman su propia escritura cada uno, y no empujan el deadline más
// lejos (así la actividad continua nunca puede dejar la persistencia
// sin escribir indefinidamente).
class PersistenceScheduler {
 public:
    // Un retraso corto, deliberadamente pequeño: suficientemente largo
    // para que una ráfaga de clicks rápidos colapse en una escritura,
    // suficientemente corto para que un crash poco después del último
    // cambio pierda a lo sumo este tanto de progreso (el shutdown
    // limpio siempre flushea de inmediato sin importar este deadline —
    // ver src/app/SpikeApp.cpp). Ver docs/PERSISTENCE.md para la
    // justificación.
    static constexpr double kDefaultDebounceMs = 2000.0;

    explicit PersistenceScheduler(double debounceMs = kDefaultDebounceMs);

    // Marca el estado como necesitado de un flush. No hace nada si ya
    // está dirty — el deadline ya programado desde el *primer* cambio
    // pendiente es lo que gobierna cuándo ocurre la (única, coalescida)
    // escritura.
    void MarkDirty(double nowMs);

    bool IsDirty() const { return dirty_; }

    // El timestamp (mismo reloj/época que `nowMs` en toda esta clase)
    // en el que debería ocurrir un flush pendiente — nullopt cuando
    // nada está dirty. Esto es lo que src/app/SpikeApp.cpp integra en
    // su cálculo de tiempo de espera del event loop, junto a los
    // deadlines de animación y de acción pasiva.
    std::optional<double> NextFlushDeadlineMs() const;

    // Llamar después de que un intento de flush tiene éxito: limpia el
    // estado dirty.
    void OnFlushSucceeded();

    // Llamar después de que un intento de flush falla: el estado
    // permanece dirty (el cambio pendiente no se descarta en
    // silencio), pero el próximo reintento se difiere otro
    // `debounceMs` en vez de reintentarse en el despertar
    // inmediatamente siguiente del event loop — acota la frecuencia de
    // reintento a como máximo una vez por intervalo de debounce incluso
    // bajo un fallo persistente.
    void OnFlushFailed(double nowMs);

 private:
    double debounceMs_;
    bool dirty_ = false;
    double deadlineMs_ = 0.0;
};

}  // namespace nimvlets::persistence
