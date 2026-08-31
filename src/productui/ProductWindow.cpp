#include "productui/ProductWindow.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "platform/TransparentWindowSupport.h"
#include "productui/UiPaint.h"

namespace nimvlets::productui {

namespace {

// Tamaño de contenido objetivo (block brief 06 §6 -> 06.1 §18: subido
// a ~800x560 para que el hero + gallery respiren). Redimensionable —
// nada acá asume que no cambia.
constexpr int kDefaultW = 800;
constexpr int kDefaultH = 560;
constexpr int kMinW = 600;
constexpr int kMinH = 460;

bool KeycodeShift(SDL_Keymod mod) {
    return (mod & SDL_KMOD_SHIFT) != 0;
}

}  // namespace

ProductWindow::~ProductWindow() {
    Close();
}

bool ProductWindow::Open(const catalog::PetCatalog& catalog) {
    if (window_ != nullptr) {
        FocusWindow();
        return true;
    }

    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow("Nimvlets", kDefaultW, kDefaultH, flags);
    if (window_ == nullptr) {
        SDL_Log("nimvlets: ProductWindow: SDL_CreateWindow failed: %s", SDL_GetError());
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
    // Carga eager (una sola vez) los artefactos ".nvprev" livianos de
    // todas las entradas del catálogo — unos pocos MB, sin abrir ningún
    // pack de animación (Block 06.2 §4/§7). El Shop usa las MISMAS
    // previews (Block 07 §21) — no se carga nada extra por sección.
    previews_->LoadBundle(catalog);
    view_ = CollectionView{};
    shopView_ = ShopView{};
    settingsView_ = SettingsView{};
    onboardingView_ = OnboardingView{};
    onboarding_ = false;
    section_ = ProductSection::kCollection;

    RecomputeScale();
    pendingExpose_ = true;
    FocusWindow();

    int wx = 0;
    int wy = 0;
    SDL_GetWindowPosition(window_, &wx, &wy);
    SDL_Log("nimvlets: Product UI window opened (%dx%d logical at (%d,%d), scale %.2f)", kDefaultW, kDefaultH, wx, wy,
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
    shopView_ = ShopView{};
    settingsView_ = SettingsView{};
    onboardingView_ = OnboardingView{};
    onboarding_ = false;
    section_ = ProductSection::kCollection;
    SDL_Log("nimvlets: Product UI window closed (resources released; pet runtime unaffected)");
}

void ProductWindow::FocusWindow() {
    if (window_ == nullptr) {
        return;
    }
    // Nimvlets corre como accessory app para el pet; activar la app hace
    // que esta ventana normal reciba teclado y clicks de contenido sin
    // el "primer click solo activa" del sistema (block brief §6/§23).
    platform::BringApplicationToForeground();
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

void ProductWindow::SetModels(
    const catalog::CollectionModel& collection,
    const catalog::ShopModel& shop,
    std::uint64_t clickBalance) {
    view_.SetModel(collection, clickBalance);
    shopView_.SetModel(shop, clickBalance);
}

void ProductWindow::SetLanguage(core::Language language) {
    view_.SetLanguage(language);
    shopView_.SetLanguage(language);
    settingsView_.SetLanguage(language);
    onboardingView_.SetLanguage(language);
}

void ProductWindow::SetPreferences(const core::Preferences& prefs) {
    settingsView_.SetPreferences(prefs);
}

void ProductWindow::EnterOnboarding(catalog::OnboardingOffer offer) {
    onboarding_ = true;
    onboardingView_.SetOffer(std::move(offer));
    onboardingView_.OnEnter();
    pendingExpose_ = true;
    SDL_Log("nimvlets: Product UI entered ONBOARDING mode (first-run starter selection)");
}

void ProductWindow::ExitOnboarding() {
    if (!onboarding_) {
        return;
    }
    onboarding_ = false;
    onboardingView_ = OnboardingView{};
    section_ = ProductSection::kCollection;
    pendingExpose_ = true;
    SDL_Log("nimvlets: Product UI left onboarding mode (starter chosen)");
}

void ProductWindow::RevealOnboardingSecret() {
    if (!onboarding_) {
        return;
    }
    onboardingView_.RevealSecret();
}

bool ProductWindow::ActiveViewDirty() const {
    if (onboarding_) {
        return onboardingView_.Dirty();
    }
    switch (section_) {
        case ProductSection::kShop:
            return shopView_.Dirty();
        case ProductSection::kSettings:
            return settingsView_.Dirty();
        case ProductSection::kCollection:
            return view_.Dirty();
    }
    return false;
}

void ProductWindow::ShowSectionForQA(ProductSection section) {
    section_ = section;
    if (section_ == ProductSection::kShop) {
        shopView_.OnEnterSection();
    } else if (section_ == ProductSection::kSettings) {
        settingsView_.OnEnterSection();
    }
    pendingExpose_ = true;
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
                shopView_.OnViewportChanged();
                settingsView_.OnViewportChanged();
                onboardingView_.OnViewportChanged();
                break;
            default:
                break;
        }
        return out;
    }

    // Solo eventos de input de ESTA ventana llegan a la sección visible.
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_KEY_DOWN:
            break;
        default:
            return out;
    }

    // Modo ONBOARDING (Block 09A): el gate de primer arranque se come
    // TODO el input; la navegación de secciones no existe hasta que se
    // elige un starter (brief §19). Un close de ventana igual se
    // reporta — src/app decide re-enfocar en vez de cerrar (brief §25).
    if (onboarding_) {
        OnboardingViewResult r;
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != myId) return out;
                out.consumed = true;
                r = onboardingView_.OnMouseMove(event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.windowID != myId || event.button.button != SDL_BUTTON_LEFT) return out;
                out.consumed = true;
                r = onboardingView_.OnMouseDown(event.button.x, event.button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID != myId) return out;
                out.consumed = true;
                r = onboardingView_.OnWheel(event.wheel.y);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.windowID != myId) return out;
                out.consumed = true;
                r = onboardingView_.OnKey(static_cast<int>(event.key.key), KeycodeShift(event.key.mod));
                break;
        }
        if (r.hasSelection) {
            out.hasOnboardingSelection = true;
            out.onboardingSelection = r.selection;
        }
        return out;
    }

    // Rutea el input a la sección visible. Los tres ViewResult comparten
    // dirty/requestClose/switchSection; solo difieren en el "verbo"
    // (activar un pet / comprar uno / cambiar una preferencia) — se
    // copia el que corresponda.
    bool wantSwitch = false;
    ProductSection switchTo = ProductSection::kCollection;

    if (section_ == ProductSection::kShop) {
        ShopViewResult r;
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != myId) return out;
                out.consumed = true;
                r = shopView_.OnMouseMove(event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.windowID != myId || event.button.button != SDL_BUTTON_LEFT) return out;
                out.consumed = true;
                r = shopView_.OnMouseDown(event.button.x, event.button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID != myId) return out;
                out.consumed = true;
                r = shopView_.OnWheel(event.wheel.y);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.windowID != myId) return out;
                out.consumed = true;
                r = shopView_.OnKey(static_cast<int>(event.key.key), KeycodeShift(event.key.mod));
                break;
        }
        if (r.hasPurchase) {
            out.hasPurchase = true;
            out.purchase = r.purchase;
        }
        if (r.requestClose) {
            out.closeRequested = true;
        }
        wantSwitch = r.switchSection;
        switchTo = r.targetSection;
    } else if (section_ == ProductSection::kSettings) {
        SettingsViewResult r;
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != myId) return out;
                out.consumed = true;
                r = settingsView_.OnMouseMove(event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.windowID != myId || event.button.button != SDL_BUTTON_LEFT) return out;
                out.consumed = true;
                r = settingsView_.OnMouseDown(event.button.x, event.button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID != myId) return out;
                out.consumed = true;
                r = settingsView_.OnWheel(event.wheel.y);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.windowID != myId) return out;
                out.consumed = true;
                r = settingsView_.OnKey(static_cast<int>(event.key.key), KeycodeShift(event.key.mod));
                break;
        }
        if (r.hasChange) {
            out.hasPreferenceChange = true;
            out.preferenceChange = r.change;
        }
        if (r.requestClose) {
            out.closeRequested = true;
        }
        wantSwitch = r.switchSection;
        switchTo = r.targetSection;
    } else {
        CollectionViewResult r;
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != myId) return out;
                out.consumed = true;
                r = view_.OnMouseMove(event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.windowID != myId || event.button.button != SDL_BUTTON_LEFT) return out;
                out.consumed = true;
                r = view_.OnMouseDown(event.button.x, event.button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID != myId) return out;
                out.consumed = true;
                r = view_.OnWheel(event.wheel.y);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.windowID != myId) return out;
                out.consumed = true;
                r = view_.OnKey(static_cast<int>(event.key.key), KeycodeShift(event.key.mod));
                break;
        }
        if (r.hasActivate) {
            out.hasActivate = true;
            out.activate = r.activate;
        }
        if (r.requestClose) {
            out.closeRequested = true;
        }
        wantSwitch = r.switchSection;
        switchTo = r.targetSection;
    }

    if (wantSwitch && switchTo != section_) {
        section_ = switchTo;
        if (section_ == ProductSection::kShop) {
            shopView_.OnEnterSection();
        } else if (section_ == ProductSection::kSettings) {
            settingsView_.OnEnterSection();
        }
        pendingExpose_ = true;  // redibujar la sección nueva
    }
    return out;
}

void ProductWindow::DrawFrame() {
    int logicalW = kDefaultW;
    int logicalH = kDefaultH;
    SDL_GetWindowSize(window_, &logicalW, &logicalH);

    UiPainter painter(renderer_, scale_);
    const float w = static_cast<float>(logicalW);
    const float h = static_cast<float>(logicalH);
    if (onboarding_) {
        // El onboarding usa el arte `.nvprev` de los candidatos (mismo
        // bundle liviano que la Collection) — NUNCA abre un `.nvpack`
        // (brief §26).
        onboardingView_.Render(painter, *text_, *previews_, w, h);
        onboardingView_.ClearDirty();
    } else if (section_ == ProductSection::kShop) {
        shopView_.Render(painter, *text_, *previews_, w, h);
        shopView_.ClearDirty();
    } else if (section_ == ProductSection::kSettings) {
        // Settings no usa previews (no carga ningún `.nvpack` — brief §23).
        settingsView_.Render(painter, *text_, w, h);
        settingsView_.ClearDirty();
    } else {
        view_.Render(painter, *text_, *previews_, w, h);
        view_.ClearDirty();
    }
}

bool ProductWindow::CaptureToBmpForQA(const std::string& path) {
    if (window_ == nullptr || renderer_ == nullptr) {
        return false;
    }
    pendingExpose_ = false;
    DrawFrame();
    // Leer ANTES de presentar: tras SDL_RenderPresent el contenido del
    // backbuffer de un renderer acelerado (Metal) queda indefinido.
    SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
    SDL_RenderPresent(renderer_);
    if (shot == nullptr) {
        SDL_Log("nimvlets: ProductWindow: SDL_RenderReadPixels failed: %s", SDL_GetError());
        return false;
    }
    const bool ok = SDL_SaveBMP(shot, path.c_str());
    if (!ok) {
        SDL_Log("nimvlets: ProductWindow: SDL_SaveBMP('%s') failed: %s", path.c_str(), SDL_GetError());
    } else {
        SDL_Log("nimvlets: ProductWindow: [dev-shot] wrote %s (%dx%d)", path.c_str(), shot->w, shot->h);
    }
    SDL_DestroySurface(shot);
    return ok;
}

void ProductWindow::RenderIfNeeded() {
    if (window_ == nullptr || renderer_ == nullptr) {
        return;
    }
    if (!pendingExpose_ && !ActiveViewDirty()) {
        return;
    }
    pendingExpose_ = false;
    DrawFrame();
    SDL_RenderPresent(renderer_);
}

}  // namespace nimvlets::productui
