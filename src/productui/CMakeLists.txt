# Product UI (Block 06). Dos librerías:
#
#   nimvlets_productui_core  — PURA (sin SDL): foco de teclado, layout de
#     la Collection, formateo de números. Se compila en cualquier
#     plataforma y la ejercita tests/ directamente, igual que src/core /
#     src/catalog.
#
#   nimvlets_productui       — capa SDL: painter, caches de texto/arte,
#     CollectionView, ProductWindow. Depende de la costura de
#     rasterizado de texto (platform/TextRasterizer.h) que resuelve el
#     ejecutable final (src/app enlaza el adapter de plataforma).
#
# Ver docs/PRODUCT_UI.md.

add_library(nimvlets_productui_core STATIC
  UiGeometry.h
  UiColor.h
  FocusList.h
  FocusList.cpp
  Format.h
  Format.cpp
  PetAccent.h
  PetAccent.cpp
  PetEditorial.h
  PetEditorial.cpp
  PreviewArtifact.h
  PreviewArtifact.cpp
  CollectionLayout.h
  CollectionLayout.cpp
)

target_include_directories(nimvlets_productui_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nimvlets_productui_core PUBLIC nimvlets_catalog PRIVATE nimvlets_warnings)

add_library(nimvlets_productui STATIC
  UiTheme.h
  UiPaint.h
  UiPaint.cpp
  TextCache.h
  TextCache.cpp
  PetPreviewCache.h
  PetPreviewCache.cpp
  CollectionView.h
  CollectionView.cpp
  ProductWindow.h
  ProductWindow.cpp
)

target_include_directories(nimvlets_productui PUBLIC ${CMAKE_SOURCE_DIR}/src)

# nimvlets_content: PetPreviewCache::SetActive recibe un
# content::FrameDefinition (el frame de reposo del pet activo). Las
# previews del resto salen del bundle liviano ".nvprev", sin abrir packs.
# nimvlets_productui_core + nimvlets_catalog llegan por ahí también.
# SDL3 es PUBLIC porque los headers exponen SDL_Renderer*/SDL_Event.
target_link_libraries(nimvlets_productui
  PUBLIC nimvlets_productui_core nimvlets_content SDL3::SDL3
  PRIVATE nimvlets_warnings
)
