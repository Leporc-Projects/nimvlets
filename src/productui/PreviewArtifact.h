#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nimvlets::productui {

// Una vista previa estática y liviana para el Product UI (la Collection):
// solo lo que hace falta para dibujar el arte del hero/gallery de un
// Nimvlet, sin cargar su pack de animación completo (Block 06.2 §4-§5).
// Formato en disco: "NVPREV1" — ver tools/compile_pet_preview.py para el
// productor y el layout exacto. Lo genera el pipeline de assets a partir
// del frame de reposo canónico del pack; NUNCA se decodifica un PNG ni
// se abre un .nvpack en runtime para esto.
//
// Puro (sin SDL): vive en nimvlets_productui_core para que los tests lo
// verifiquen con buffers sintéticos chicos, igual que
// content::PetPackLoader. El pipeline de dibujo sube `rgba` como
// cualquier otra textura RGBA straight-alpha.
struct PetPreviewImage {
    std::string petId;
    std::string variantId;   // "" si el pet no tiene variantes
    std::string sourcePack;  // basename del .nvpack de origen (procedencia/diagnóstico)

    int width = 0;
    int height = 0;

    // RGBA8, row-major, top-to-bottom, straight alpha. Tamaño exacto
    // width * height * 4.
    std::vector<std::uint8_t> rgba;
};

// Parsea un "NVPREV1" desde memoria. Falla ruidosamente (devuelve false
// y un mensaje humano en `outError`) ante cualquier problema estructural:
// magic inválido, versión no soportada, datos truncados, dimensiones no
// positivas, o un `pixel_bytes` que no coincide con width*height*4 o con
// los bytes que quedan. `outImage` queda sin especificar al fallar —
// siempre revisar el booleano.
bool LoadPetPreviewFromMemory(
    const std::uint8_t* data, std::size_t size, PetPreviewImage& outImage, std::string& outError);

// Lee `path` a memoria y llama a LoadPetPreviewFromMemory(). Devuelve
// false (con `outError`) si el archivo no se puede abrir/leer, reportado
// igual que un fallo de contenido — el llamador solo necesita el
// booleano.
bool LoadPetPreviewFromFile(const std::string& path, PetPreviewImage& outImage, std::string& outError);

// La convención de nombres que mapea una entrada de catálogo a su
// artefacto de preview: cambia un sufijo ".nvpack" por ".nvprev"; si
// `packPath` no termina en ".nvpack", agrega ".nvprev". Es la MISMA
// regla que tools/compile_pet_previews.py — así el catálogo no necesita
// ningún campo nuevo ni migración de esquema (Block 06.2 §5).
std::string PreviewPathForPack(const std::string& packPath);

}  // namespace nimvlets::productui
