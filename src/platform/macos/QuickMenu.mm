#include "platform/SystemShell.h"
#include "platform/QuickMenuModel.h"

#include <SDL3/SDL.h>

#import <AppKit/AppKit.h>

#include <memory>

// Menú rápido nativo de macOS: un NSStatusItem en la barra de menús del
// sistema (block brief §14). El NSMenu se construye a partir de
// platform::BuildQuickMenuModel() — el mismo modelo puro que cubre
// tests/QuickMenuModelTest.cpp — así que la estructura y las etiquetas
// que se testean son literalmente las que se envían.
//
// Sin ARC (igual que el resto de src/platform/macos). Nada acá pide un
// permiso de TCC: un NSStatusItem es UI de nuestra propia app. El icono
// es un mark monocromo de primera parte, dibujado por código y marcado
// como replaceable (ver MakeMenuBarIcon()).

// --- Icono de desarrollo, REEMPLAZABLE -------------------------------
//
// Un mark monocromo simple (silueta de criatura sentada), dibujado
// programáticamente y marcado como template para que macOS lo tinte
// según el tema de la barra de menús. No es el icono de marca final —
// cuando exista un asset branded, se reemplaza esta función por una
// carga de recurso. Ver docs/PRODUCT_UI.md §6.
static NSImage* MakeMenuBarIcon() {
    const CGFloat s = 18.0;
    NSImage* image = [NSImage imageWithSize:NSMakeSize(s, s)
                                    flipped:NO
                             drawingHandler:^BOOL(NSRect /*rect*/) {
        [[NSColor blackColor] setFill];
        // Cuerpo: una "bean" redondeada.
        NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(3.0, 2.0, 12.0, 11.0)
                                                            xRadius:5.5
                                                            yRadius:5.5];
        [body fill];
        // Dos orejitas.
        NSBezierPath* earL = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(4.5, 10.5, 3.5, 6.0)];
        NSBezierPath* earR = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(10.0, 10.5, 3.5, 6.0)];
        [earL fill];
        [earR fill];
        return YES;
    }];
    [image setTemplate:YES];
    return image;
}

// --- Target Objective-C de los items del menú -----------------------

@interface NimvletsMenuTarget : NSObject {
@public
    uint32_t userEventType;
}
- (void)fire:(id)sender;
@end

@implementation NimvletsMenuTarget
- (void)fire:(id)sender {
    NSInteger tag = [(NSMenuItem*)sender tag];
    SDL_Event event;
    SDL_zero(event);
    event.type = userEventType;
    event.user.type = userEventType;
    event.user.code = (Sint32)tag;
    SDL_PushEvent(&event);
}
@end

namespace nimvlets::platform {

namespace {

NSMenu* BuildNSMenu(const QuickMenuModel& model, NimvletsMenuTarget* target);

NSMenuItem* MakeItem(const MenuItem& item, NimvletsMenuTarget* target) {
    if (item.kind == MenuItemKind::kSeparator) {
        return [NSMenuItem separatorItem];
    }

    NSString* label = [NSString stringWithUTF8String:item.label.c_str()];
    NSMenuItem* nsItem = [[[NSMenuItem alloc] initWithTitle:label action:nil keyEquivalent:@""] autorelease];

    switch (item.kind) {
        case MenuItemKind::kHeader:
            [nsItem setEnabled:NO];
            break;
        case MenuItemKind::kAction:
        case MenuItemKind::kCheckable:
            [nsItem setTarget:target];
            [nsItem setAction:@selector(fire:)];
            [nsItem setTag:(NSInteger)item.action];
            [nsItem setEnabled:item.enabled ? YES : NO];
            if (item.kind == MenuItemKind::kCheckable) {
                [nsItem setState:item.checked ? NSControlStateValueOn : NSControlStateValueOff];
            }
            break;
        case MenuItemKind::kSubmenu: {
            QuickMenuModel sub;
            sub.items = item.submenu;
            [nsItem setSubmenu:BuildNSMenu(sub, target)];
            [nsItem setEnabled:YES];
            break;
        }
        case MenuItemKind::kSeparator:
            break;  // ya manejado arriba
    }
    return nsItem;
}

NSMenu* BuildNSMenu(const QuickMenuModel& model, NimvletsMenuTarget* target) {
    NSMenu* menu = [[[NSMenu alloc] init] autorelease];
    [menu setAutoenablesItems:NO];  // respetar setEnabled:NO de los headers
    for (const MenuItem& item : model.items) {
        [menu addItem:MakeItem(item, target)];
    }
    return menu;
}

class MacQuickMenu final : public SystemShell {
 public:
    ~MacQuickMenu() override { Shutdown(); }

    bool Install(std::uint32_t userEventType) override {
        if (statusItem_ != nil) {
            return true;
        }
        userEventType_ = userEventType;
        target_ = [[NimvletsMenuTarget alloc] init];
        target_->userEventType = userEventType;

        statusItem_ = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength] retain];
        statusItem_.button.image = MakeMenuBarIcon();
        statusItem_.button.toolTip = @"Nimvlets";
        Rebuild();
        SDL_Log("nimvlets: macOS quick menu installed (NSStatusItem)");
        return true;
    }

    void SetState(const ShellState& state) override {
        state_ = state;
        if (statusItem_ != nil) {
            Rebuild();
        }
    }

    void Shutdown() override {
        if (statusItem_ != nil) {
            [[NSStatusBar systemStatusBar] removeStatusItem:statusItem_];
            [statusItem_ release];
            statusItem_ = nil;
        }
        if (target_ != nil) {
            [target_ release];
            target_ = nil;
        }
    }

 private:
    void Rebuild() {
        const QuickMenuModel model = BuildQuickMenuModel(state_);
        statusItem_.menu = BuildNSMenu(model, target_);
    }

    NSStatusItem* statusItem_ = nil;
    NimvletsMenuTarget* target_ = nil;
    ShellState state_;
    std::uint32_t userEventType_ = 0;
};

}  // namespace

std::unique_ptr<SystemShell> CreateSystemShell() {
    return std::make_unique<MacQuickMenu>();
}

}  // namespace nimvlets::platform
