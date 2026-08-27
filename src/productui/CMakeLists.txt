# Product UI (Block 06). Dos librerías:
#
#   nimvlets_productui_core  — PURA (sin SDL): foco de teclado + layout
#     de la Collection. Se compila en cualquier plataforma y la ejercita
#     tests/ directamente, igual que src/core / src/catalog.
#
#   nimvlets_productui       — capa SDL: painter, caches de texto/arte,
#     CollectionView, ProductWindow. (Se agrega en el commit siguiente
#     de este bloque.)
#
# Ver docs/PRODUCT_UI.md.

add_library(nimvlets_productui_core STATIC
  UiGeometry.h
  FocusList.h
  FocusList.cpp
  CollectionLayout.h
  CollectionLayout.cpp
)

target_include_directories(nimvlets_productui_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_productui_core PUBLIC nimvlets_catalog PRIVATE nimvlets_warnings)
