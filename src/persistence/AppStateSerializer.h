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
// formato. `ownedPetIds` se escribe siempre en orden canónico
// (ascendente, sin duplicados, sin string vacío) sin importar cómo
// venga en `state` — ver NormalizeOwnedPetIds.
std::vector<std::uint8_t> SerializeAppState(const AppState& state);

// Ordena ascendente, elimina duplicados y descarta cualquier string
// vacío de `ids`. src/app lo usa al mutar AppState::ownedPetIds; el
// serializer lo aplica sobre una copia para que la salida sea siempre
// canónica. Idempotente.
void NormalizeOwnedPetIds(std::vector<std::string>& ids);

// Falla ruidosamente (retorna false, `outError` con un mensaje
// específico) ante un magic inválido, datos truncados, o un
// schemaVersion que esta build no sabe leer. Block 06 amplió lo
// legible de "exactamente la versión actual" a "1 o 2": un archivo v1
// (Block 03/04/05) se lee con su layout viejo y los campos nuevos de
// v2 quedan en su default — `outState.schemaVersion` se fija a la
// versión actual, así que el próximo Save() lo reescribe como v2. Una
// versión más nueva desconocida (o basura) sigue tratándose como "no
// se puede usar este dato". `outState` queda en un estado no
// especificado si falla; siempre verificar el valor de retorno.
bool DeserializeAppState(const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError);

}  // namespace nimvlets::persistence
