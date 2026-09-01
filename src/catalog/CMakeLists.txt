add_library(nimvlets_catalog STATIC
  PetIdentity.h
  PetEntitlement.h
  PetEntitlement.cpp
  PetCatalog.h
  PetCatalog.cpp
  PetCatalogLoader.h
  PetCatalogLoader.cpp
  ActivePetResolution.h
  ActivePetResolution.cpp
  CollectionModel.h
  CollectionModel.cpp
  ShopModel.h
  ShopModel.cpp
  PurchasePolicy.h
  PurchasePolicy.cpp
  OnboardingPolicy.h
  OnboardingPolicy.cpp
  StarterShopModel.h
  StarterShopModel.cpp
  StarterPurchasePolicy.h
  StarterPurchasePolicy.cpp
)

target_include_directories(nimvlets_catalog PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Depende de nimvlets_content (PetDefinition + LoadPetPackFromFile,
# usados por ActivePetResolution) — nada de SDL, nada de persistence:
# la conexión entre selección de catálogo y persistencia la hace
# src/app, no esta librería (ver docs/CATALOG.md).
target_link_libraries(nimvlets_catalog PUBLIC nimvlets_content PRIVATE nimvlets_warnings)
