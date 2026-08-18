#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "persistence/AppState.h"

namespace nimvlets::persistence {

// (De)serialización binaria, determinista y versionada de AppState —
// ver docs/PERSISTENCE.md para el layout exacto en disco ("NVSTATE1").
// Pura: sin I/O de archivos aquí (ver AppStateStore.h para eso), así
// que es directamente testeable con buffers de bytes en memoria — la
// misma separación que content::PetPackLoader estableció en Block 02
// entre parseo y acceso a archivos.

// Determinista: llamar esto dos veces sobre un AppState sin cambios
// produce una salida idéntica byte a byte — sin timestamps, sin
// padding, sin orden de iteración de map/set en ningún lugar del
// formato.
std::vector<std::uint8_t> SerializeAppState(const AppState& state);

// Falla ruidosamente (retorna false, `outError` con un mensaje
// específico) ante un magic inválido, datos truncados, o un
// schemaVersion que no coincide exactamente con
// AppState::kCurrentSchemaVersion — este bloque no tiene ruta de
// migración para un schema más viejo o más nuevo, así que cualquier
// desajuste se trata como "no se puede usar este dato" en vez de
// adivinarlo. `outState` queda en un estado no especificado si falla;
// siempre verificar el valor de retorno.
bool DeserializeAppState(const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError);

}  // namespace nimvlets::persistence
