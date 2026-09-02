#include "core/DisplayControls.h"

#include <algorithm>
#include <cmath>

namespace nimvlets::core {

const char* PetSizeChoiceId(PetSizeChoice choice) {
    switch (choice) {
        case PetSizeChoice::kSmall:
            return "small";
        case PetSizeChoice::kMedium:
            return "medium";
        case PetSizeChoice::kLarge:
            return "large";
    }
    return "medium";
}

PetSizeChoice ParsePetSizeChoice(std::string_view id) {
    if (id == "small") {
        return PetSizeChoice::kSmall;
    }
    if (id == "large") {
        return PetSizeChoice::kLarge;
    }
    // "medium", vacío, o cualquier otra cosa -> el tamaño neutro.
    return PetSizeChoice::kMedium;
}

double PetSizeScaleFactor(PetSizeChoice choice) {
    switch (choice) {
        case PetSizeChoice::kSmall:
            return 0.80;
        case PetSizeChoice::kMedium:
            return 1.00;
        case PetSizeChoice::kLarge:
            // Block 06.1: bajado de 1.30 a 1.15 — QA del owner: 1.30
            // agranda demasiado los sprites detallados. El id persistido
            // "large" sigue resolviendo acá, así que una preferencia
            // guardada se re-interpreta sola al nuevo factor (sin
            // migración). Ver DEC-114.
            return 1.15;
    }
    return 1.00;
}

int NormalizeOpacityPercent(int rawPercent) {
    int best = 100;
    int bestDistance = -1;
    for (const int choice : kOpacityChoicesPercent) {
        const int distance = std::abs(choice - rawPercent);
        if (bestDistance < 0 || distance < bestDistance) {
            bestDistance = distance;
            best = choice;
        }
    }
    return best;
}

float OpacityFraction(int normalizedPercent) {
    const int clamped = std::clamp(normalizedPercent, 0, 100);
    return static_cast<float>(clamped) / 100.0f;
}

WindowTopLeft SafePetPlacement(DisplayBounds display, int petW, int petH) {
    // Centrado, igual que SDL_WINDOWPOS_CENTERED sobre este display.
    int x = display.x + (display.w - petW) / 2;
    int y = display.y + (display.h - petH) / 2;

    // Acotado: si el pet CABE en el eje, el rectángulo entero queda
    // dentro del display; si NO cabe (pet más grande que el display en
    // ese eje), el centrado deja el borde superior/izquierdo fuera de
    // pantalla — se ancla ese borde al del display en su lugar. El
    // resultado nunca deja la esquina superior-izquierda fuera de
    // los límites del display.
    if (petW <= display.w) {
        x = std::clamp(x, display.x, display.x + display.w - petW);
    } else {
        x = display.x;
    }
    if (petH <= display.h) {
        y = std::clamp(y, display.y, display.y + display.h - petH);
    } else {
        y = display.y;
    }
    return WindowTopLeft{x, y};
}

}  // namespace nimvlets::core
