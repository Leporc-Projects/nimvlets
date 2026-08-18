#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nimvlets::persistence {

// Última posición en pantalla de la ventana, en el mismo espacio de
// coordenadas que usan SDL_GetWindowPosition()/SDL_SetWindowPosition()
// (int, puntos lógicos de pantalla) — ver el manejo de fin-de-drag en
// src/app/SpikeApp.cpp.
struct WindowPosition {
    int x = 0;
    int y = 0;

    friend bool operator==(WindowPosition a, WindowPosition b) {
        return a.x == b.x && a.y == b.y;
    }
};

// El conjunto completo de estado de aplicación local, en disco, que
// persiste Block 03. Datos puros — sin SDL, sin I/O de archivos, sin
// código de plataforma (ver AppStateSerializer.h para la
// (de)serialización y AppStateStore.h para la política de
// almacenamiento que lo lee/escribe). Deliberadamente mínimo: solo
// campos con significado real en el runtime de este bloque — ver
// docs/PERSISTENCE.md para qué se dejó fuera a propósito y por qué.
//
// Genérico por construcción: activePetId/activeVariantId son simples
// strings, no un enum de Nimvlets conocidos — agregar un nuevo pet id
// o variante más adelante nunca requiere tocar este struct, ni
// AppStateSerializer, ni AppStateStore.
struct AppState {
    // Solo se incrementa cuando la forma en disco de este struct
    // cambia de manera no retrocompatible. Ver AppStateSerializer.cpp
    // para cómo se maneja un desajuste (defaults seguros, nunca un
    // crash, sin lógica de migración en este bloque).
    static constexpr std::uint32_t kCurrentSchemaVersion = 1;

    std::uint32_t schemaVersion = kCurrentSchemaVersion;

    // La única moneda — ver AGENTS.md §2. Empieza en 0; solo se
    // incrementa por un click real. uint64 para que nunca desborde de
    // forma realista.
    std::uint64_t clickBalance = 0;

    // Qué pet está activo actualmente. String vacío = sin save aún /
    // sin definir. Este bloque no implementa *selección* de pet — ver
    // docs/PERSISTENCE.md — solo mantiene este campo sincronizado con
    // la verdad: cualquier pet que el runtime haya cargado realmente.
    std::string activePetId;

    // Qué variante de activePetId está activa, si ese pet tiene
    // variantes (ver content::PetDefinition::variantGroup). String
    // vacío = sin variante / no aplica. Nada en este bloque escribe un
    // valor no vacío aquí (todavía no existe selección de variante) —
    // el campo se conserva a través de load/save para que un bloque
    // futuro pueda poblarlo sin un cambio de schema.
    std::string activeVariantId;

    // Posición de la ventana la última vez que el usuario la movió,
    // para que la app pueda reabrir donde la dejaron. std::nullopt =
    // sin save aún / nunca se arrastró (usa el default existente de
    // centrado al iniciar).
    std::optional<WindowPosition> lastWindowPosition;

    friend bool operator==(const AppState& a, const AppState& b) {
        return a.schemaVersion == b.schemaVersion &&
               a.clickBalance == b.clickBalance &&
               a.activePetId == b.activePetId &&
               a.activeVariantId == b.activeVariantId &&
               a.lastWindowPosition == b.lastWindowPosition;
    }
};

}  // namespace nimvlets::persistence
