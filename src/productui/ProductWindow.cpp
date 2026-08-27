#include "productui/ProductWindow.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "productui/UiPaint.h"

namespace nimvlets::productui {

namespace {

// Tamaño de contenido objetivo (block brief §6). Redimensionable —
// nada acá asume que no cambia.
constexpr int kDefaultW = 760;
constexpr int kDefaultH = 540;
constexpr int kMinW = 560;
constexpr int kMinH = 420;

bool KeycodeShift(SDL_Keymod mod) {
    return (mod & SDL_KMOD_SHIFT) != 0;
}

}  // namespace

ProductWindow::~ProductWindow() {
    Close();
}

bool ProductWindow::Open(const catalog::PetCatalog& catalog) {
    catalog_ = &catalog;
    if (window_ != nullptr) {
        FocusWindow();
        return true;
    }

    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow("Nimvlets", kDefaultW, kDefaultH, flags);
    if (window_ == nullptr) {
        SDL_Log("nimvlets: ProductWindow: SDL_CreateWindow failed: %s", SDL_GetError());
        catalog_ = &catalog;
        return false;
    }
    SDL_SetWindowMinimumSize(window_, kMinW, kMinH);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    renderer_ = SDL_CreateRenderer(window_, nullptr);  // driver por defecto (acelerado) — no es una ventana con forma
    if (renderer_ == nullptr) {
        SDL_Log("nimvlets: ProductWindow: SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    text_ = std::make_unique<TextCache>(renderer_);
    previews_ = std::make_unique<PetPreviewCache>(renderer_);
    view_ = CollectionView{};

    RecomputeScale();
    pendingExpose_ = true;
    FocusWindow();

    SDL_Log("nimvlets: Product UI window opened (%dx%d logical, scale %.2f)", kDefaultW, kDefaultH,
            static_cast<double>(scale_));
    return true;
}

void ProductWindow::DestroyResources() {
    // Orden inverso: caches (dueñas de texturas) antes que el renderer.
    previews_.reset();
    text_.reset();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void ProductWindow::Close() {
    if (window_ == nullptr) {
        return;
    }
    DestroyResources();
    view_ = CollectionView{};
    SDL_Log("nimvlets: Product UI window closed (resources released; pet runtime unaffected)");
}

void ProductWindow::FocusWindow() {
    if (window_ == nullptr) {
        return;
    }
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
}

std::uint32_t ProductWindow::WindowId() const {
    return window_ != nullptr ? SDL_GetWindowID(window_) : 0;
}

void ProductWindow::RecomputeScale() {
    if (window_ == nullptr) {
        return;
    }
    int logicalW = kDefaultW;
    int logicalH = kDefaultH;
    int pixelW = kDefaultW;
    int pixelH = kDefaultH;
    SDL_GetWindowSize(window_, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);
    const float newScale = logicalW > 0 ? static_cast<float>(pixelW) / static_cast<float>(logicalW) : 1.0f;
    if (std::abs(newScale - scale_) > 0.01f) {
        scale_ = newScale;
        if (text_) {
            text_->Clear();  // los bitmaps de texto son específicos de la escala
        }
    }
}

void ProductWindow::SetModel(const catalog::CollectionModel& model, std::uint64_t clickBalance) {
    view_.SetModel(model, clickBalance);
}

void ProductWindow::SetActivePreview(
    const std::string& petId, const std::string& variantId, const content::FrameDefinition& restFrame) {
    if (previews_) {
        previews_->SetActive(petId, variantId, restFrame);
    }
}

ProductWindowEvent ProductWindow::HandleEvent(const SDL_Event& event) {
    ProductWindowEvent out;
    if (window_ == nullptr) {
        return out;
    }
    const std::uint32_t myId = SDL_GetWindowID(window_);

    // Eventos de ventana.
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
        if (event.window.windowID != myId) {
            return out;
        }
        out.consumed = true;
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                out.closeRequested = true;
                break;
            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_SHOWN:
                pendingExpose_ = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                RecomputeScale();
                view_.OnViewportChanged();
                break;
            default:
                break;
        }
        return out;
    }

    CollectionViewResult r;
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.windowID != myId) {
                return out;
            }
            out.consumed = true;
            r = view_.OnMouseMove(event.motion.x, event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.windowID != myId || event.button.button != SDL_BUTTON_LEFT) {
                return out;
            }
            out.consumed = true;
            r = view_.OnMouseDown(event.button.x, event.button.y);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.windowID != myId) {
                return out;
            }
            out.consumed = true;
            r = view_.OnWheel(event.wheel.y);
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.windowID != myId) {
                return out;
            }
            out.consumed = true;
            r = view_.OnKey(static_cast<int>(event.key.key), KeycodeShift(event.key.mod));
            break;
        default:
            return out;
    }

    if (r.hasActivate) {
        out.hasActivate = true;
        out.activate = r.activate;
    }
    if (r.requestClose) {
        out.closeRequested = true;
    }
    return out;
}

void ProductWindow::RenderIfNeeded() {
    if (window_ == nullptr || renderer_ == nullptr) {
        return;
    }
    if (!pendingExpose_ && !view_.Dirty()) {
        return;
    }
    pendingExpose_ = false;

    int logicalW = kDefaultW;
    int logicalH = kDefaultH;
    SDL_GetWindowSize(window_, &logicalW, &logicalH);

    UiPainter painter(renderer_, scale_);
    view_.Render(painter, *text_, *previews_, *catalog_, static_cast<float>(logicalW), static_cast<float>(logicalH));
    view_.ClearDirty();
    SDL_RenderPresent(renderer_);
}

}  // namespace nimvlets::productui
