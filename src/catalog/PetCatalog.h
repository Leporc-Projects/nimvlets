#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "catalog/PetIdentity.h"

namespace nimvlets::catalog {

// Cuántas entradas `StarterRole::kNormal` tiene que declarar un catálogo
// para que su onboarding de producción PUEDA armarse — la tríada
// canónica Artu / Rato / Rin Rin (brief §1/§7). El compilador de
// catálogo y el loader lo exigen cuando `productionOnboardingReady`
// viene en true; el catálogo de dev de Block 09A tiene 0.
inline constexpr std::size_t kRequiredNormalStarterCount = 3;

// Rol de una entrada de catálogo dentro del ONBOARDING de primer
// arranque (Block 09A, schema "NVCATLG1" v4). DATO, nunca una rama
// `if (pet == "artu")` en el runtime/UI (brief §7). Deliberadamente
// mínimo: sin metadatos especulativos de economía — un starter NO es
// `publiclyPurchasable` solo por serlo (brief §7), y el shop oculto de
// starters es trabajo de Block 10, no de acá.
enum class StarterRole : std::uint8_t {
    // No participa del onboarding.
    kNone = 0,
    // Candidato de starter NORMAL — la tríada Artu / Rato / Rin Rin. El
    // usuario nuevo elige exactamente uno gratis.
    kNormal = 1,
    // Candidato de starter SECRETO — Frin. Aparece discretamente tras 44
    // s de dwell en la pantalla de selección (brief §10). Un pet lógico
    // con variantes (macho / hembra): elegir Frin otorga EXACTAMENTE la
    // variante elegida (brief §5/§13).
    kSecret = 2,
};

// Una fila del catálogo: qué pet/variante es, cómo se llama, y dónde
// está su pack compilado. `packPath` se resuelve en runtime desde el
// directorio de trabajo del proceso, igual que `kPetPackPath` lo hacía
// antes de este bloque (ver docs/CATALOG.md) — no se valida como
// archivo existente al cargar el catálogo, solo como string no vacío;
// un pack faltante o corrupto se descubre y reporta claramente recién
// cuando algo intenta cargarlo de verdad (ver LoadPetForIdentity en
// ActivePetResolution.h).
struct CatalogEntry {
    PetIdentity identity;
    std::string displayName;
    std::string packPath;
    bool isDefault = false;

    // --- Metadatos de economía (Block 07, schema "NVCATLG1" v3) -------
    //
    // El precio y la visibilidad en el Shop son DATO, nunca una rama
    // `if (pet == "nidir")` en el runtime/UI (brief §10). Mínimos a
    // propósito: sin campos especulativos de economía.

    // Precio de compra en clics. 0 = sin precio / no a la venta. Una
    // entrada con publiclyPurchasable=true DEBE tener price > 0 (el
    // compilador y la política de compra rechazan precio cero — brief
    // §26). Los valores actuales son PROVISIONALES de QA/economía, no
    // balanceo final (ver docs/CATALOG.md §12).
    std::uint64_t priceClicks = 0;

    // true = esta entrada aparece en el Shop público normal como
    // comprable. Frin queda en false en las DOS variantes: su ruta de
    // obtención es onboarding + shop oculto de starters, trabajo futuro
    // que NO se implementa ni se insinúa acá (brief §11). El modelo de
    // Shop nunca la lista solo porque su entrada de catálogo exista.
    bool publiclyPurchasable = false;

    // SEMILLA de propiedad para desarrollo/default (Block 06, schema
    // "NVCATLG1" v2). NO es autoridad de runtime: solo se consulta una
    // vez, cuando persistence::AppState::ownershipSeeded todavía es
    // false, para poblar `ownedPetIds`. A partir de ahí el archivo de
    // estado manda y este campo se ignora. Un bloque futuro de
    // onboarding (Block 09) reemplaza la siembra sin tocar el catálogo.
    // Ver docs/CATALOG.md §11 y docs/PRODUCT_UI.md §5.
    //
    // Para dos entradas que comparten petId (las dos variantes de
    // Frin), la propiedad es del petId: alcanza con que UNA lo marque,
    // pero el manifest de dev las marca a las dos por claridad.
    bool initiallyOwned = false;

    // --- Onboarding (Block 09A, schema "NVCATLG1" v4) ----------------
    //
    // Rol de esta entrada en la selección de starter de primer arranque.
    // Ver StarterRole arriba y docs/ONBOARDING.md. El default `kNone` +
    // `PetCatalog::productionOnboardingReady == false` significa "este
    // catálogo no habilita onboarding de producción" — el estado del
    // catálogo de dev de Block 09A (todavía no existe contenido de
    // Artu/Rato/Rin Rin).
    StarterRole starterRole = StarterRole::kNone;
};

// Un catálogo de pets ya validado y listo para consultarse: sin ids
// duplicados y con exactamente una entrada default. Puro — sin SDL,
// sin I/O de archivos — construido a partir de una lista de entradas
// que el llamador garantiza válida.
//
// El único punto de entrada normal para construir un PetCatalog real
// es PetCatalogLoader (ver PetCatalogLoader.h), que valida el formato
// binario "NVCATLG1" antes de llamar a este constructor — ver
// docs/CATALOG.md. Este constructor en sí NO revalida (duplicados,
// default único, campos no vacíos): es responsabilidad exclusiva del
// loader, igual que content::PetDefinition no se autovalida y confía
// en content::PetPackLoader. Construir un PetCatalog directamente con
// `entries` vacío o sin ningún `isDefault` produce un catálogo cuyo
// comportamiento en Default() no está definido — solo aceptable en
// tests que construyen fixtures ya sabidas válidas a mano.
class PetCatalog {
 public:
    // Catálogo vacío/inválido — solo existe para que PetCatalogLoader
    // pueda usar el mismo patrón de out-parameter que
    // content::LoadPetPackFromMemory (rellenar un PetCatalog& ya
    // existente al éxito, dejarlo sin especificar al fallo). Nunca es
    // el resultado de una carga exitosa.
    PetCatalog() = default;

    explicit PetCatalog(std::vector<CatalogEntry> entries);

    explicit PetCatalog(std::vector<CatalogEntry> entries, bool productionOnboardingReady);

    // 3-arg (Block 09A, pasada de endurecimiento — DEC-133): agrega el
    // marcador de catálogo SINTÉTICO-DEV. Lo usa PetCatalogLoader; los
    // tests que solo necesitan `productionOnboardingReady` siguen usando
    // el ctor de 2 argumentos.
    explicit PetCatalog(
        std::vector<CatalogEntry> entries, bool productionOnboardingReady, bool devSyntheticOnboarding);

    // nullptr si `identity` no aparece en el catálogo.
    const CatalogEntry* Find(const PetIdentity& identity) const;

    // La entrada marcada is_default. Precondición: el catálogo se
    // construyó a partir de una lista válida (ver el comentario de la
    // clase) — nunca es null en un catálogo real cargado por
    // PetCatalogLoader.
    const CatalogEntry& Default() const;

    const std::vector<CatalogEntry>& Entries() const { return entries_; }

    // El datum EXPLÍCITO que arma el onboarding de PRODUCCIÓN (Block
    // 09A, schema v4). false por defecto y en el catálogo de dev actual:
    // el onboarding de producción NUNCA se muestra hasta que un bloque
    // futuro (09B) lo ponga en true — y el compilador
    // (tools/compile_pet_catalog.py) solo deja compilarlo en true si
    // existen las 3 entradas `starterRole == kNormal` con su contenido
    // (pack + .nvprev) en disco. Ver docs/ONBOARDING.md y DEC-132.
    bool ProductionOnboardingReady() const { return productionOnboardingReady_; }

    // true SOLO en el catálogo sintético del harness solo-DEV
    // (`assets/dev/onboarding_dev_catalog.nvcat`) — descriptores con
    // packs/previews ALIAS de otros Nimvlets, para ejercitar la máquina
    // de estados/UI de onboarding antes de que exista contenido de
    // Artu/Rato/Rin Rin. MUTUAMENTE EXCLUYENTE con
    // `productionOnboardingReady` (el compilador y el loader rechazan
    // ambos en true). `src/app` exige este byte para forzar el gate DEV,
    // así un alias nunca alcanza el camino de producción (DEC-133). El
    // catálogo de producción real lo tiene en false.
    bool DevSyntheticOnboarding() const { return devSyntheticOnboarding_; }

 private:
    std::vector<CatalogEntry> entries_;
    std::size_t defaultIndex_ = 0;
    bool productionOnboardingReady_ = false;
    bool devSyntheticOnboarding_ = false;
};

}  // namespace nimvlets::catalog
