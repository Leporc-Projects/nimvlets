#include "persistence/AppStateSerializer.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace nimvlets::persistence {

namespace {

// "NVSTATE1", exactamente 8 bytes — ver docs/PERSISTENCE.md. El magic
// NO cambia entre schemas: la versión vive en el uint32 que sigue,
// justamente para que subir de schema no invalide el reconocimiento de
// archivo.
constexpr char kMagic[8] = {'N', 'V', 'S', 'T', 'A', 'T', 'E', '1'};

// La versión más vieja que este build todavía sabe leer (con migración
// hacia adelante). Todo en [kOldestReadableSchema, kCurrentSchemaVersion]
// se acepta; fuera de ese rango se trata como dato inutilizable.
constexpr std::uint32_t kOldestReadableSchema = 1;

void AppendUint8(std::vector<std::uint8_t>& out, std::uint8_t v) {
    out.push_back(v);
}

void AppendUint32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendUint64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    std::uint8_t bytes[8];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendInt32(std::vector<std::uint8_t>& out, std::int32_t v) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &v, sizeof(v));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void AppendString(std::vector<std::uint8_t>& out, const std::string& s) {
    AppendUint32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

// Cursor con verificación de límites, misma disciplina de "fallar en
// el primer error, recordar solo ese" que ByteReader de
// content::PetPackLoader (ver src/content/PetPackLoader.cpp) — los
// enteros se leen vía memcpy crudo y por lo tanto son little-endian en
// la plataforma que lee, igual que las funciones Append* de arriba y
// todo otro formato en disco de este repositorio (solo x86_64/arm64 —
// ver docs/ANIMATION_RUNTIME.md).
class ByteReader {
 public:
    ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    const std::string& Error() const { return error_; }
    bool Ok() const { return ok_; }

    bool ReadBytes(void* dst, std::size_t n) {
        if (!ok_) {
            return false;
        }
        if (n > size_ - pos_) {
            Fail("unexpected end of app-state data");
            return false;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return true;
    }

    bool ReadUint8(std::uint8_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint32(std::uint32_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadUint64(std::uint64_t& out) { return ReadBytes(&out, sizeof(out)); }
    bool ReadInt32(std::int32_t& out) { return ReadBytes(&out, sizeof(out)); }

    bool ReadString(std::string& out) {
        std::uint32_t len = 0;
        if (!ReadUint32(len)) {
            return false;
        }
        out.assign(static_cast<std::size_t>(len), '\0');
        if (len == 0) {
            return true;
        }
        return ReadBytes(out.data(), len);
    }

    void Fail(const std::string& message) {
        if (ok_) {  // conserva solo el primer fallo — los siguientes suelen ser ruido derivado de ese
            ok_ = false;
            error_ = message;
        }
    }

 private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    bool ok_ = true;
    std::string error_;
};

// Lee la parte que v1 y v2 comparten (todo lo de Block 03): balance,
// pet activo, y posición de ventana. Deja `outState` con los campos v2
// en su default; el caller de v2 los sobrescribe después.
bool ReadCommonV1Body(ByteReader& reader, AppState& outState) {
    if (!reader.ReadUint64(outState.clickBalance) || !reader.ReadString(outState.activePetId) ||
        !reader.ReadString(outState.activeVariantId)) {
        return false;
    }
    std::uint8_t hasPosition = 0;
    std::int32_t posX = 0;
    std::int32_t posY = 0;
    if (!reader.ReadUint8(hasPosition) || !reader.ReadInt32(posX) || !reader.ReadInt32(posY)) {
        return false;
    }
    if (hasPosition != 0) {
        outState.lastWindowPosition = WindowPosition{posX, posY};
    }
    return true;
}

}  // namespace

void NormalizeOwnedEntitlements(std::vector<OwnedEntitlement>& ents) {
    // Una entrada con petId vacío nunca es un pet real — se descarta en
    // vez de persistirse (defensivo: un archivo hecho a mano podría
    // traerla).
    ents.erase(std::remove_if(ents.begin(), ents.end(),
                              [](const OwnedEntitlement& e) { return e.petId.empty(); }),
               ents.end());
    std::sort(ents.begin(), ents.end(), [](const OwnedEntitlement& a, const OwnedEntitlement& b) {
        if (a.petId != b.petId) {
            return a.petId < b.petId;
        }
        return a.variantId < b.variantId;
    });
    ents.erase(std::unique(ents.begin(), ents.end()), ents.end());
}

std::vector<std::uint8_t> SerializeAppState(const AppState& state) {
    // Copia local solo para poder normalizar el orden de
    // ownedEntitlements sin exigir que el caller ya lo haya hecho — la
    // salida es siempre canónica (ordenada, sin duplicados), así que el
    // formato sigue siendo determinista byte a byte.
    std::vector<OwnedEntitlement> ownedEnts = state.ownedEntitlements;
    NormalizeOwnedEntitlements(ownedEnts);

    std::vector<std::uint8_t> out;
    out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
    // Siempre se escribe con el schema actual, sin importar qué versión
    // trajera el AppState en memoria (un v1 recién migrado incluido).
    AppendUint32(out, AppState::kCurrentSchemaVersion);

    // --- Cuerpo común v1 ---
    AppendUint64(out, state.clickBalance);
    AppendString(out, state.activePetId);
    AppendString(out, state.activeVariantId);
    AppendUint8(out, state.lastWindowPosition.has_value() ? 1 : 0);
    AppendInt32(out, state.lastWindowPosition ? state.lastWindowPosition->x : 0);
    AppendInt32(out, state.lastWindowPosition ? state.lastWindowPosition->y : 0);

    // --- Añadido de v2 (Block 06) ---
    AppendUint8(out, state.ownershipSeeded ? 1 : 0);
    // v4 (Block 07): la lista de propiedad pasa de petIds sueltos a
    // pares (petId, variantId). El resto del bloque v2 (lock/size/
    // opacity) y el v3 (language) no cambian de layout.
    AppendUint32(out, static_cast<std::uint32_t>(ownedEnts.size()));
    for (const OwnedEntitlement& ent : ownedEnts) {
        AppendString(out, ent.petId);
        AppendString(out, ent.variantId);
    }
    AppendUint8(out, state.lockPosition ? 1 : 0);
    AppendString(out, state.sizeChoice);
    AppendUint32(out, state.opacityPercent);

    // --- Añadido de v3 (Block 06.1) ---
    AppendString(out, state.language);

    // --- Añadido de v5 (Block 09A) ---
    // El lifecycle de primer arranque, un solo byte. Ver AppState.h /
    // docs/ONBOARDING.md. NUNCA se escribe ningún detalle transitorio del
    // UI de onboarding (brief §4).
    AppendUint8(out, static_cast<std::uint8_t>(state.onboardingLifecycle));

    // --- Añadido de v6 (Block 11A) ---
    // El modo de conteo de clics PEDIDO ("" / "nimvlet_only" /
    // "anywhere"). Un string, igual que sizeChoice/language. Es LO ÚNICO
    // que esta feature persiste: nunca coordenadas, timestamps,
    // historial ni estado de permiso (docs/PRIVACY_SECURITY.md §H).
    AppendString(out, state.clickCountingMode);

    return out;
}

namespace {
// Mapea el byte persistido del lifecycle. Un valor fuera de {0,1,2}
// (archivo v5 editado a mano, o de una build futura leída dentro del
// rango) se trata como `kLegacyComplete`: la opción NO DESTRUCTIVA —
// "este estado tiene datos, no lo mandes a onboarding" — en vez de
// rechazar el archivo entero y perder la propiedad/balance del usuario.
OnboardingLifecycle ParseOnboardingLifecycleByte(std::uint8_t b) {
    switch (b) {
        case 0:
            return OnboardingLifecycle::kPending;
        case 2:
            return OnboardingLifecycle::kCompleted;
        default:
            return OnboardingLifecycle::kLegacyComplete;
    }
}
}  // namespace

bool DeserializeAppState(
    const std::uint8_t* data, std::size_t size, AppState& outState, std::string& outError,
    std::uint32_t* outOnDiskSchemaVersion) {
    ByteReader reader(data, size);

    char magic[8];
    if (!reader.ReadBytes(magic, sizeof(magic))) {
        outError = reader.Error();
        return false;
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        outError = "not a valid Nimvlets app-state file (bad magic)";
        return false;
    }

    std::uint32_t schemaVersion = 0;
    if (!reader.ReadUint32(schemaVersion)) {
        outError = reader.Error();
        return false;
    }
    // Todo en [1, kCurrentSchemaVersion] es legible con migración hacia
    // adelante. Fuera de ese rango (una build más nueva que escribió v4,
    // o basura) se trata como "no se puede usar este dato".
    if (schemaVersion < kOldestReadableSchema || schemaVersion > AppState::kCurrentSchemaVersion) {
        outError = "app-state schema version " + std::to_string(schemaVersion) +
                   " is not a readable version (this build reads " + std::to_string(kOldestReadableSchema) +
                   " through " + std::to_string(AppState::kCurrentSchemaVersion) + ")";
        return false;
    }

    AppState state;
    // Siempre queda marcado como el schema actual: un archivo más viejo
    // se re-escribe con el schema actual en el próximo Save() (migración
    // hacia adelante, una sola vez, sin lógica de conversión más allá de
    // "los campos nuevos arrancan en su default").
    state.schemaVersion = AppState::kCurrentSchemaVersion;

    if (!ReadCommonV1Body(reader, state)) {
        outError = reader.Error();
        return false;
    }

    // Bloque v2 (Block 06): propiedad + preferencias del menú rápido.
    if (schemaVersion >= 2) {
        std::uint8_t seeded = 0;
        std::uint32_t ownedCount = 0;
        if (!reader.ReadUint8(seeded) || !reader.ReadUint32(ownedCount)) {
            outError = reader.Error();
            return false;
        }
        state.ownershipSeeded = seeded != 0;
        state.ownedEntitlements.reserve(ownedCount);
        if (schemaVersion >= 4) {
            // v4: pares (petId, variantId).
            for (std::uint32_t i = 0; i < ownedCount; ++i) {
                OwnedEntitlement ent;
                if (!reader.ReadString(ent.petId) || !reader.ReadString(ent.variantId)) {
                    outError = reader.Error();
                    return false;
                }
                state.ownedEntitlements.push_back(std::move(ent));
            }
        } else {
            // v2/v3: petIds sueltos. Migración: cada petId poseído se
            // vuelve una autorización de PET ENTERO ({petId, ""}) — así
            // un Frin de Block 06 (que exponía macho y hembra) sigue
            // dando las dos variantes (brief §5).
            for (std::uint32_t i = 0; i < ownedCount; ++i) {
                std::string id;
                if (!reader.ReadString(id)) {
                    outError = reader.Error();
                    return false;
                }
                state.ownedEntitlements.push_back(OwnedEntitlement{std::move(id), std::string()});
            }
        }
        std::uint8_t locked = 0;
        if (!reader.ReadUint8(locked) || !reader.ReadString(state.sizeChoice) ||
            !reader.ReadUint32(state.opacityPercent)) {
            outError = reader.Error();
            return false;
        }
        state.lockPosition = locked != 0;
        NormalizeOwnedEntitlements(state.ownedEntitlements);
    }

    // Bloque v3 (Block 06.1): idioma del Product UI.
    if (schemaVersion >= 3) {
        if (!reader.ReadString(state.language)) {
            outError = reader.Error();
            return false;
        }
    }

    // Bloque v5 (Block 09A): lifecycle de primer arranque.
    if (schemaVersion >= 5) {
        std::uint8_t lifecycleByte = 0;
        if (!reader.ReadUint8(lifecycleByte)) {
            outError = reader.Error();  // v5 truncado antes del byte -> falla, no adivina
            return false;
        }
        state.onboardingLifecycle = ParseOnboardingLifecycleByte(lifecycleByte);
    } else {
        // MIGRACIÓN v1/v2/v3/v4 -> v5: cualquier estado que precede al
        // onboarding es un usuario EXISTENTE, ya onboardeado. Se preserva
        // TODO lo demás exacto; solo se le pone este flag (brief §3.A /
        // §4). Distinto del default `kPending` de un `AppState{}` sin
        // save, que representa un usuario potencialmente nuevo.
        state.onboardingLifecycle = OnboardingLifecycle::kLegacyComplete;
    }

    // Bloque v6 (Block 11A): modo de conteo de clics PEDIDO.
    if (schemaVersion >= 6) {
        if (!reader.ReadString(state.clickCountingMode)) {
            outError = reader.Error();  // v6 truncado antes del string -> falla, no adivina
            return false;
        }
    }
    // MIGRACIÓN v1..v5 -> v6: el campo simplemente queda vacío, y
    // core::ParseClickCountingMode("") == kNimvletOnly. Es la invariante
    // del brief §12: NINGÚN usuario existente queda con conteo global
    // habilitado por actualizar, y nada acá toca propiedad, balance,
    // lifecycle ni preferencias. La reconciliación histórica de
    // propiedad sigue gateada por `< kFirstExplicitEntitlementSchema`
    // (== 4) en src/app — un umbral fijo que este bump NO mueve
    // (DEC-129 / DEC-139).

    // schemaVersion == 1: los campos v2/v3/v4 quedan en su default.
    // schemaVersion == 2: `language` queda "" (src/app lo resuelve
    // desde el locale del OS en el próximo arranque); la propiedad se
    // parsea provisionalmente a `{petId, ""}` y src/app la reconcilia.
    // schemaVersion == 3: idem propiedad; `language` se conserva.
    // schemaVersion <= 4: `onboardingLifecycle` = kLegacyComplete.
    // schemaVersion <= 5: `clickCountingMode` queda "" -> kNimvletOnly.

    outState = std::move(state);
    outError.clear();
    // La versión EN DISCO, antes de normalizar `outState.schemaVersion`
    // a la actual — src/app decide con esto si corre la reconciliación
    // de propiedad legacy (DEC-129).
    if (outOnDiskSchemaVersion != nullptr) {
        *outOnDiskSchemaVersion = schemaVersion;
    }
    return true;
}

}  // namespace nimvlets::persistence
