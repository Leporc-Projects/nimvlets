#pragma once

#include <string>
#include <string_view>

namespace nimvlets::core {

// Controles de despliegue a nivel de USUARIO que Block 06 expone por el
// menú rápido nativo (tamaño, opacidad, bloqueo de posición) — política
// pura, sin SDL, sin AppKit: la misma separación que
// core::ClickThroughPolicy estableció (una decisión con nombre propio y
// testeable en vez de constantes sueltas dentro de SpikeApp). Ver
// docs/PRODUCT_UI.md §7.
//
// Genérico por construcción: nada acá conoce un pet concreto. El
// tamaño de usuario es un MULTIPLICADOR encima de
// content::PetDefinition::visualScale (dato de contenido, congelado en
// Block 05) — nunca lo reemplaza ni lo edita. "Medium" es exactamente
// 1.0, así que un owner que nunca toca el control ve el pet al tamaño
// que su contenido declara, sin cambio de comportamiento respecto de
// antes de este bloque.

// Conjunto finito y cerrado de tamaños de usuario (block brief §15:
// "prefer a small finite set such as Small/Medium/Large"). Se persiste
// como string legible (ver persistence::AppState::sizeChoice), no como
// el valor del enum, para que el archivo de estado siga siendo
// inspeccionable y a prueba de futuro.
enum class PetSizeChoice {
    kSmall,
    kMedium,
    kLarge,
};

// String estable de cada opción — lo que va al archivo de estado y al
// menú. Nunca traducir estos identificadores (AGENTS.md §17).
const char* PetSizeChoiceId(PetSizeChoice choice);

// Parsea el string persistido. Cualquier valor desconocido/vacío cae a
// kMedium (misma disciplina de "defaults seguros, nunca un crash" que
// persistence::DeserializeAppState) — así un archivo de estado de una
// versión futura con un tamaño que esta build no conoce simplemente se
// ve al tamaño neutro en vez de fallar.
PetSizeChoice ParsePetSizeChoice(std::string_view id);

// El multiplicador que este tamaño aplica ENCIMA de visualScale.
// Valores deliberados y documentados (docs/PRODUCT_UI.md §7):
//   kSmall  = 0.80  — cuatro quintos, "se aparta un poco"
//   kMedium = 1.00  — exactamente el tamaño que declara el contenido
//   kLarge  = 1.15  — un poco más grande (bajado de 1.30 en Block 06.1
//                     tras QA del owner: 1.30 estiraba los sprites
//                     detallados — DEC-114)
double PetSizeScaleFactor(PetSizeChoice choice);

// --- Opacidad -------------------------------------------------------

// Conjunto finito de opacidades de ventana ofrecidas en el menú, de
// más opaca a más translúcida. 55% es el piso: por debajo el pet se
// vuelve difícil de encontrar y de clickear.
inline constexpr int kOpacityChoicesPercent[] = {100, 85, 70, 55};

// Ajusta `rawPercent` a la opción válida más cercana. Fuera de rango o
// sin sentido -> 100 (totalmente opaco, el default). Determinista.
int NormalizeOpacityPercent(int rawPercent);

// Fracción [0.0, 1.0] lista para SDL_SetWindowOpacity(), a partir de un
// porcentaje ya normalizado.
float OpacityFraction(int normalizedPercent);

// --- Bloqueo de posición -----------------------------------------

// Si un gesto de ARRASTRE puede empezar, dado el preferencia de
// bloqueo. Block brief §16: bloqueado => el pet no se puede arrastrar,
// pero click / hover / click-through / animaciones siguen intactos —
// por eso esto gobierna SOLO el inicio del drag, nada más. Trivial a
// propósito: existe para que el invariante viva en un lugar con nombre
// y con test, no repetido como un `if` en SpikeApp.
inline bool PetDragAllowed(bool lockPosition) { return !lockPosition; }

// --- Colocación segura de la ventana (Block 11B) -------------------
//
// La acción "Reset position" de Settings (brief §6-§8) devuelve una
// ventana del pet que quedó fuera de pantalla, o difícil de encontrar
// tras un cambio de monitores, a un lugar conocido y seguro. Su
// semántica NO inventa coordenadas: es la MISMA colocación por defecto
// que el arranque usa cuando no hay posición guardada
// (SDL_WINDOWPOS_CENTERED), acotada al display objetivo. Esta es la
// pieza pura de esa política — sin SDL: src/app resuelve el display que
// contiene el Product UI y sus bounds, y esto calcula el destino.

// Rectángulo de un display en el MISMO espacio de coordenadas que
// SDL_GetWindowPosition()/SDL_GetDisplayBounds() (int, puntos lógicos
// de pantalla; el origen puede ser no-cero en un setup multi-monitor).
struct DisplayBounds {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// Esquina superior-izquierda para SDL_SetWindowPosition().
struct WindowTopLeft {
    int x = 0;
    int y = 0;
};

// La colocación SEGURA canónica de una ventana de `petW` x `petH`
// dentro de `display`: centrada (idéntico a SDL_WINDOWPOS_CENTERED, el
// default de arranque), y además ACOTADA para que el rectángulo entero
// quede dentro del display. Si el pet es MÁS grande que el display en
// algún eje, se ancla ese borde al del display (nunca fuera de pantalla
// por arriba/izquierda). Determinista y pura: mismas entradas -> mismo
// resultado, sin importar si el pet está oculto o si Lock Position está
// activo (eso lo decide src/app, no esta geometría).
WindowTopLeft SafePetPlacement(DisplayBounds display, int petW, int petH);

}  // namespace nimvlets::core
