#pragma once

#include <string>

#include "core/ClickCounting.h"

// Tipos PUROS del monitor de clics globales (Block 11A) — el mismo
// patrón que SystemShellTypes.h establece para el menú rápido: sin SDL,
// sin AppKit, sin windows.h, sin X11, para que la POLÍTICA
// (¿qué muestra Settings?) sea testeable en cualquier host y para que el
// Product UI nunca contenga una rama por plataforma (brief §18).
//
// Este archivo es también la frontera de privacidad declarada del
// bloque: acá no hay —y no puede haber— ningún tipo que transporte
// coordenadas, timestamps, identidad de app/ventana, o cualquier otra
// cosa que un evento global podría llevar. Ver docs/GLOBAL_CLICK_MODE.md
// y docs/PRIVACY_SECURITY.md §H.

namespace nimvlets::platform {

// Qué puede hacer ESTA plataforma/backend respecto de observar
// pasivamente la presión del botón primario en todo el sistema.
enum class GlobalClickCapability {
    // No hay forma legítima de hacerlo acá (o todavía no está
    // implementada/verificada). Nunca se finge lo contrario.
    kUnavailable,
    // Se puede, pero el OS lo gatea detrás de un permiso explícito del
    // usuario (macOS: Input Monitoring).
    kSupportedNeedsPermission,
    // Se puede sin ningún permiso del OS.
    kSupportedNoPermission,
};

// Estado del permiso requerido, consultado SIN provocar ningún diálogo.
enum class GlobalClickPermission {
    // La capacidad no pide permiso (o no hay capacidad).
    kNotRequired,
    kGranted,
    // El preflight dio negativo. Deliberadamente UN solo valor: macOS no
    // distingue "nunca se pidió" de "se pidió y se denegó" desde el
    // proceso, y fingir esa distinción produciría UI que miente.
    kNotGranted,
    // No se pudo consultar (API ausente en una versión vieja del OS).
    kUnknown,
};

// Los hechos observables del adapter nativo. Todo lo que Settings sabe
// del mundo nativo pasa por acá.
struct GlobalClickStatus {
    GlobalClickCapability capability = GlobalClickCapability::kUnavailable;
    GlobalClickPermission permission = GlobalClickPermission::kNotRequired;
    // ¿El monitor está corriendo AHORA? Es el hecho, consultado al
    // adapter — no una deducción de la preferencia persistida. Es la
    // entrada del modo EFECTIVO (core::ResolveEffectiveClickCounting).
    bool monitorActive = false;
    // Se intentó Start() en esta sesión y falló (o el monitor se cayó).
    bool startFailed = false;
    // El nombre del permiso TAL COMO lo llama el sistema operativo —
    // "Input Monitoring" en macOS. Lo provee el adapter para que la copy
    // de Settings pueda nombrarlo sin ningún `#ifdef __APPLE__` en
    // src/productui (brief §18). "" cuando no aplica.
    std::string permissionName;
};

// La línea de estado, corta y humana, que Settings dibuja bajo el
// control (brief §9 — sin panel de administración).
enum class GlobalClickStatusLine {
    kNone,                // no hay nada útil que decir (modo local, todo normal)
    kActive,              // "Active"
    kPermissionRequired,  // "Permission needed"
    kUnavailable,         // "Not available on this system"
    kFailed,              // "Could not start"
};

// Lo que Settings consume: estado GENÉRICO, derivado de la capacidad —
// nunca del nombre de la plataforma (brief §18).
struct GlobalClickUiState {
    // ¿El segmento "Anywhere" se puede elegir? false en una plataforma
    // sin capacidad: el segmento se sigue DIBUJANDO (para que la línea
    // de estado tenga a qué referirse) pero no es accionable.
    bool anywhereSelectable = false;
    GlobalClickStatusLine statusLine = GlobalClickStatusLine::kNone;
    // ¿Ofrecer un reintento manual? El camino robusto cuando el OS exige
    // que el usuario vaya a los Ajustes del Sistema — ver
    // docs/GLOBAL_CLICK_MODE.md §7 (deliberadamente NO se usa un deep
    // link no documentado).
    bool showCheckAgain = false;
    // Copia de GlobalClickStatus::permissionName, para que Settings
    // reciba UNA sola estructura y pueda sustituir "{permission}" en su
    // copy sin conocer la plataforma.
    std::string permissionName;
};

GlobalClickUiState ResolveGlobalClickUiState(
    core::ClickCountingMode requested, const GlobalClickStatus& status);

// Qué debe pasar cuando el owner elige "Anywhere" en Settings. Pura, y
// la razón por la que src/app no decide esto con un `if` propio: el
// contrato de "nunca pedir permiso sin una acción explícita del owner Y
// una explicación previa" (brief §8) es una regla de producto.
enum class GlobalClickRequestOutcome {
    // Sin permiso de por medio (o ya concedido): se aplica directo.
    kApplyDirectly,
    // Hay que MOSTRAR la explicación de primera parte primero. Solo
    // "Continue" puede llamar al pedido nativo.
    kNeedsExplanation,
    // La plataforma no lo soporta: el pedido se ignora.
    kUnavailable,
};

GlobalClickRequestOutcome EvaluateGlobalClickRequest(const GlobalClickStatus& status);

}  // namespace nimvlets::platform
