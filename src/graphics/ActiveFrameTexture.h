#pragma once

#include "content/AnimationDefinition.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace nimvlets::graphics {

// UNA sola textura reutilizable para el frame que se está mostrando
// ahora mismo, en vez de una SDL_Texture por CADA frame de CADA
// animación/dirección/estado del pet activo (el modelo de
// graphics::FrameTexture, Block 02).
//
// Por qué (Block 05, pasada de estabilización -- ver DEC-081):
//   - Residencia: un pet real tiene 152-204 frames compilados; el
//     modelo por-frame mantiene ESE número de texturas GPU vivas todo
//     el tiempo, aunque como mucho una esté en pantalla.
//   - Presentación: cada avance de frame cambiaba el SDL_Texture* que
//     se dibuja. Acá el objeto textura NUNCA cambia entre frames -- solo
//     su contenido (SDL_UpdateTexture), así que el renderer/compositor
//     ve un único recurso estable durante toda una animación.
//
// Precondición estructural (verificada en este bloque contra los 4
// packs reales): TODOS los frames de un pet compilado comparten las
// mismas dimensiones en píxeles, porque compose_on_canvas() los coloca
// a todos sobre el mismo canvas de trabajo compartido y el downscale de
// runtime aplica el mismo factor. EnsureSize() igualmente recrea la
// textura si alguna vez dejara de cumplirse (p. ej. al cambiar de pet),
// así que esto no es una suposición silenciosa.
class ActiveFrameTexture {
public:
    ActiveFrameTexture() = default;
    ~ActiveFrameTexture();

    ActiveFrameTexture(const ActiveFrameTexture&) = delete;
    ActiveFrameTexture& operator=(const ActiveFrameTexture&) = delete;

    // Sube `frame` a la textura activa, creándola/recreándola solo si
    // las dimensiones cambiaron respecto de la actual. Si `frame` es el
    // MISMO frame ya subido, no re-sube nada (el caso común: un redraw
    // de confirmación, o un present repetido del mismo frame).
    // Retorna false y loguea vía SDL_Log si algo falla.
    bool SetFrame(SDL_Renderer* renderer, const content::FrameDefinition& frame);

    // La textura viva, o nullptr si todavía no se subió ningún frame.
    SDL_Texture* Get() const { return texture_; }

    // Destruye la textura y olvida el frame subido -- se llama al
    // cambiar de pet y en Shutdown().
    void Reset();

    // Solo para tests/diagnóstico: cuántas veces se creó una textura
    // nueva (debe quedarse en 1 por pet, no crecer por frame).
    int TextureCreationCount() const { return creationCount_; }

private:
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    // Identidad del último frame subido -- puntero, no copia: los
    // FrameDefinition viven en pet_ y son estables mientras el pet no
    // cambie (y Reset() se llama en cada cambio de pet).
    const content::FrameDefinition* uploaded_ = nullptr;
    int creationCount_ = 0;
};

}  // namespace nimvlets::graphics
