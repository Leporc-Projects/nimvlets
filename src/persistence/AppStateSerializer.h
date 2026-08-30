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
// formato. `ownedEntitlements` se escribe siempre en orden canónico
// (ordenado por (petId, variantId), sin duplicados, sin petId vacío)
// sin importar cómo venga en `state` — ver NormalizeOwnedEntitlements.
std::vector<std::uint8_t> SerializeAppState(const AppState& state);

// Ordena por (petId, variantId), elimina duplicados exactos y descarta
// cualquier entrada con petId vacío. src/app aplica además la
// canonicalización SEMÁNTICA completa (subsunción de variantes) vía
// catalog::CanonicalizePetEntitlements antes de asignar; el serializer
// solo necesita un orden determinista, y lo aplica sobre una copia.
// Idempotente.
void NormalizeOwnedEntitlements(std::vector<OwnedEntitlement>& ents);

// Falla ruidosamente (retorna false, `outError` con un mensaje
// específico) ante un magic inválido, datos truncados, o un
// schemaVersion que esta build no sabe leer. Con migración hacia
// adelante lee cualquier versión en [1, kCurrentSchemaVersion] (hoy
// 1, 2, 3 y 4): un archivo más viejo se lee con su layout, los campos
// de versiones posteriores quedan en su default (o se derivan del
// viejo, como la propiedad al pasar de `ownedPetIds` v1-3 a
// `ownedEntitlements` v4), y `outState.schemaVersion` se fija a la
// versión actual, así que el próximo Save() lo reescribe al formato
// actual. Una versión más nueva desconocida (o basura) sigue
// tratándose como "no se puede usar este dato". `outState` queda en un
// estado no especificado si falla; siempre verificar el valor de
// retorno.
//
// `outOnDiskSchemaVersion` (opcional): la versión que traía el archivo
// EN DISCO, ANTES de que `outState.schemaVersion` se normalice a la
// actual. Solo se escribe cuando el parseo tiene éxito. src/app la usa
// para decidir si hay que correr la reconciliación de propiedad legacy
// (`catalog::ExpandHistoricalWholePetEntitlements`) — que solo aplica a
// un estado que GENUINAMENTE venía del modelo "por pet lógico" de los
// schemas v1..v3, nunca a un v4 (DEC-129).
bool DeserializeAppState(
    const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError,
    std::uint32_t* outOnDiskSchemaVersion = nullptr);

}  // namespace nimvlets::persistence
