#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    // para cómo se maneja un desajuste.
    //
    // v1 (Block 03): clickBalance + activePetId/Variant + windowPos.
    // v2 (Block 06): agrega el estado de propiedad (ownedPetIds +
    //   ownershipSeeded) y las preferencias del menú rápido
    //   (lockPosition, sizeChoice, opacityPercent).
    // v3 (Block 06.1): agrega `language` ("en"/"es").
    //
    // Cada subida trae una migración hacia adelante mínima: un archivo
    // más viejo se lee con su layout, los campos nuevos quedan en su
    // default, y `schemaVersion` se marca como el actual para que el
    // próximo Save() lo reescriba — así el click balance, la posición,
    // la propiedad y las preferencias del owner sobreviven cada
    // actualización. Ver docs/PERSISTENCE.md §3 y DEC-109/DEC-116.
    static constexpr std::uint32_t kCurrentSchemaVersion = 3;

    std::uint32_t schemaVersion = kCurrentSchemaVersion;

    // La única moneda — ver AGENTS.md §2. Empieza en 0; solo se
    // incrementa por un click real. Block 06 la MUESTRA dentro del
    // Product UI pero sigue sin poder gastarla (no hay Shop todavía).
    // uint64 para que nunca desborde de forma realista.
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

    // --- Estado de propiedad (Block 06) ------------------------------
    //
    // Qué Nimvlets posee el owner, por `petId` (nunca por variante —
    // Frin es UN Nimvlet lógico: poseer "frin" da acceso a macho y
    // hembra). Orden canónico: ordenado ascendente y sin duplicados,
    // impuesto por el serializer y por src/app al mutar — así el
    // archivo es determinista y dos AppState con el mismo conjunto
    // siempre comparan igual.
    //
    // Autoridad una vez escrito; NO se deriva del catálogo en cada
    // arranque. El catálogo solo aporta la SEMILLA de desarrollo (ver
    // catalog::CatalogEntry::initiallyOwned) y solo cuando
    // `ownershipSeeded` todavía es false — ver más abajo.
    std::vector<std::string> ownedPetIds;

    // false = este estado nunca pasó por la inicialización de
    // propiedad. En ese caso src/app siembra `ownedPetIds` desde las
    // entradas `initiallyOwned` del catálogo y pone esto en true. Un
    // bloque futuro de onboarding (Block 09) reemplaza esa siembra por
    // la elección real de starter del jugador SIN un cambio de schema:
    // solo escribe `ownedPetIds` + `ownershipSeeded = true` con su
    // propia lógica. "Poseer cero Nimvlets" (todo bloqueado) y "nunca
    // se inicializó" son estados distintos precisamente por este flag.
    bool ownershipSeeded = false;

    // --- Preferencias del menú rápido (Block 06) -------------------
    //
    // Ver docs/PRODUCT_UI.md §7 y core::DisplayControls, que traduce
    // estos a comportamiento genérico de runtime (ninguna rama por
    // pet).

    // Si true, la ventana del pet no se puede arrastrar (click / hover
    // / click-through / animaciones siguen intactos — block brief §16).
    bool lockPosition = false;

    // Tamaño de usuario, MULTIPLICADOR encima de visualScale. String
    // legible ("small"/"medium"/"large"); un valor desconocido se
    // interpreta como "medium" al leer (core::ParsePetSizeChoice).
    // Vacío se trata igual que "medium".
    std::string sizeChoice;

    // Opacidad de la ventana del pet, porcentaje. 0 = "sin preferencia
    // guardada" -> se trata como 100 (totalmente opaco) al aplicar;
    // cualquier otro valor se ajusta al conjunto finito del menú
    // (core::NormalizeOpacityPercent).
    std::uint32_t opacityPercent = 0;

    // --- Idioma del Product UI (Block 06.1) -------------------------
    //
    // "" = el owner nunca eligió idioma explícitamente. En ese caso
    // src/app resuelve el inicial desde el locale del OS (en/es), pero
    // NO lo persiste — así "el owner eligió inglés" y "adivinamos
    // inglés" siguen siendo distinguibles. Una vez que elige desde el
    // menú Language, se escribe "en"/"es" acá y su preferencia gana
    // siempre (brief §5). Un valor desconocido se interpreta como "en"
    // al leer (core::ParseLanguage). Ver docs/PRODUCT_UI.md §16.
    std::string language;

    friend bool operator==(const AppState& a, const AppState& b) {
        return a.schemaVersion == b.schemaVersion &&
               a.clickBalance == b.clickBalance &&
               a.activePetId == b.activePetId &&
               a.activeVariantId == b.activeVariantId &&
               a.lastWindowPosition == b.lastWindowPosition &&
               a.ownedPetIds == b.ownedPetIds &&
               a.ownershipSeeded == b.ownershipSeeded &&
               a.lockPosition == b.lockPosition &&
               a.sizeChoice == b.sizeChoice &&
               a.opacityPercent == b.opacityPercent &&
               a.language == b.language;
    }
};

}  // namespace nimvlets::persistence
