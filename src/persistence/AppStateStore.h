#pragma once

#include <cstdint>
#include <string>

#include "persistence/AppState.h"

namespace nimvlets::persistence {

// Lee/escribe un AppState en un único archivo dentro de
// `directoryPath`, con una estrategia de escritura atómica (escribir a
// un archivo temporal, luego renombrar sobre el real) para que un save
// interrumpido a mitad de camino (crash, corte de energía, disco
// lleno) nunca deje un archivo corrupto a medio escribir en lugar de
// uno previo válido. Ver docs/PERSISTENCE.md para la política exacta
// de nombre de archivo/ruta.
//
// Esta clase asume que `directoryPath` ya existe — en producción,
// SDL_GetPrefPath() garantiza esto (crea el directorio ella misma);
// los tests deben crear su propio directorio temporal antes de
// construir esta clase. Nunca resuelve una ruta por sí misma y nunca
// toca nada fuera del único archivo (más su archivo de staging
// `.tmp`) dentro de `directoryPath` — ver tests/AppStateStoreTest.cpp,
// que la apunta a directorios temporales aislados y frescos, nunca a
// la ubicación real de app-data del usuario.
class AppStateStore {
 public:
    explicit AppStateStore(std::string directoryPath);

    // Retorna un estado previamente guardado, o AppState{} (defaults
    // seguros) si aún no existe ningún save, el archivo no se puede
    // leer, o su contenido no parsea (magic inválido, truncado, schema
    // version no soportada). Nunca lanza excepción, nunca crashea al
    // llamador. Si `outWarning` no es nulo, se setea a una razón
    // corta, específica y legible cada vez que el resultado no es "se
    // encontró un save válido con el schema actual" (y se limpia en
    // caso contrario) — el llamador decide si/cómo loguearlo.
    //
    // Si `outOnDiskSchemaVersion` no es nulo, se inicializa a
    // AppState::kCurrentSchemaVersion y, SOLO cuando se parsea un save
    // válido, se sobre-escribe con la versión que traía el archivo en
    // disco. En cualquier otro caso (sin save, ilegible, corrupto)
    // queda en el valor actual — así "no hay estado legacy que migrar".
    // Ver DEC-129.
    //
    // Si `outSaveFileExisted` no es nulo, se pone en `true` cuando había
    // un archivo de estado en disco (aunque no se pueda leer o parsear),
    // y `false` SOLO cuando genuinamente no existía. src/app lo usa para
    // distinguir un USUARIO NUEVO (sin archivo -> onboarding, cuando esté
    // armado) de una RECUPERACIÓN de un archivo corrupto (existía -> se
    // trata como usuario existente, NUNCA se lo manda a onboarding —
    // brief §4 / §27, DEC-131).
    AppState Load(
        std::string* outWarning = nullptr, std::uint32_t* outOnDiskSchemaVersion = nullptr,
        bool* outSaveFileExisted = nullptr) const;

    // Serializa `state` y lo escribe atómicamente: el contenido
    // completo se escribe primero a un archivo temporal en el mismo
    // directorio (así el rename final es en el mismo filesystem, que
    // es lo que lo hace atómico), y solo se renombra a su lugar si esa
    // escritura tuvo éxito completo. Si algo falla — el archivo
    // temporal no se puede crear o escribir, o el rename falla — el
    // archivo previamente guardado (si existe) queda completamente
    // intacto, y esto retorna false con `outError` seteado a una razón
    // específica. Nunca lanza excepción, nunca crashea.
    bool Save(const AppState& state, std::string& outError) const;

 private:
    std::string StatePath() const;
    std::string TempPath() const;

    std::string directoryPath_;
};

}  // namespace nimvlets::persistence
