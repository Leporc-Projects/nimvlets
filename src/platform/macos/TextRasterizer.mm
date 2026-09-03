#include "platform/TextRasterizer.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

// Rasterizador de texto de macOS: Core Text sobre la fuente del
// sistema (SF Pro). Straight alpha de salida — ver
// platform/TextRasterizer.h. Nada de esto requiere permiso de TCC:
// crear una fuente y dibujar en un CGBitmapContext de nuestra propia
// memoria es puramente cómputo local (AGENTS.md §5).
//
// Este target NO usa ARC (igual que TransparentWindowSupport.mm): los
// NSObject* que devuelve el runtime vienen autoreleased y NO se
// liberan a mano; los objetos CF creados por CT*/CG* (CTLine, CGColor,
// CGColorSpace, CGContext) sí se CFRelease-an explícitamente. Todo el
// cuerpo corre dentro de un @autoreleasepool.

namespace nimvlets::platform {

namespace {

// NSFont autoreleased -> CTFontRef prestado (toll-free bridged). NO se
// libera: lo maneja el @autoreleasepool del caller.
//
// `family == kSerif` pide el *diseño serif del sistema* (New York) vía
// NSFontDescriptor — NO se carga ningún asset. Si esta versión de macOS
// no puede resolver la variante serif, cae a la sans de UI: nunca queda
// sin fuente.
CTFontRef BorrowSystemFont(double pixelSize, TextWeight weight, TextFamily family) {
    NSFontWeight nsWeight = NSFontWeightRegular;
    switch (weight) {
        case TextWeight::kRegular:
            nsWeight = NSFontWeightRegular;
            break;
        case TextWeight::kMedium:
            nsWeight = NSFontWeightMedium;
            break;
        case TextWeight::kSemibold:
            nsWeight = NSFontWeightSemibold;
            break;
    }
    NSFont* font = [NSFont systemFontOfSize:pixelSize weight:nsWeight];
    if (font == nil) {
        font = [NSFont systemFontOfSize:pixelSize];
    }
    if (family == TextFamily::kSerif && font != nil) {
        NSFontDescriptor* serifDesc =
            [font.fontDescriptor fontDescriptorWithDesign:NSFontDescriptorSystemDesignSerif];
        NSFont* serif = serifDesc != nil ? [NSFont fontWithDescriptor:serifDesc size:pixelSize] : nil;
        if (serif != nil) {
            font = serif;
        }
    }
    return (CTFontRef)font;
}

// Reemplaza saltos de línea por espacios — el Product UI de Block 06 no
// tiene texto multilínea y CTLine solo maneja una línea de todos modos.
// Devuelve un NSString autoreleased.
NSString* SingleLine(const std::string& utf8) {
    NSString* raw = [[[NSString alloc] initWithBytes:utf8.data()
                                              length:utf8.size()
                                            encoding:NSUTF8StringEncoding] autorelease];
    if (raw == nil) {
        return @"";
    }
    return [[raw componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]]
        componentsJoinedByString:@" "];
}

// Atributos de una línea: fuente, color y, si `kernPx > 0`,
// interletraje extra (kCTKernAttributeName, en unidades de píxel del
// backing — el caller ya lo escaló). Devuelve un NSDictionary
// autoreleased.
NSDictionary* LineAttrs(CTFontRef font, CGColorRef color, double kernPx) {
    if (kernPx > 0.0) {
        return @{
            (NSString*)kCTFontAttributeName : (id)font,
            (NSString*)kCTForegroundColorAttributeName : (id)color,
            (NSString*)kCTKernAttributeName : @(kernPx),
        };
    }
    return @{
        (NSString*)kCTFontAttributeName : (id)font,
        (NSString*)kCTForegroundColorAttributeName : (id)color,
    };
}

// CTLine creada (el caller la CFRelease-a). `color` es prestado.
CTLineRef CreateLine(NSString* text, CTFontRef font, CGColorRef color, double kernPx) {
    NSAttributedString* attributed =
        [[[NSAttributedString alloc] initWithString:text
                                        attributes:LineAttrs(font, color, kernPx)] autorelease];
    return CTLineCreateWithAttributedString((CFAttributedStringRef)attributed);
}

// Devuelve una CTLine (el caller la CFRelease-a) recortada al final con
// "…" si `line` supera `maxWidthPx`; si entra o no se puede truncar,
// devuelve `line` con una referencia extra.
CTLineRef TruncatedLine(CTLineRef line, CTFontRef font, CGColorRef color, int maxWidthPx, double kernPx) {
    if (maxWidthPx <= 0) {
        return (CTLineRef)CFRetain(line);
    }
    double ascent = 0.0;
    double descent = 0.0;
    double leading = 0.0;
    const double naturalWidth = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    if (naturalWidth <= static_cast<double>(maxWidthPx)) {
        return (CTLineRef)CFRetain(line);
    }

    NSAttributedString* ellipsisAttr =
        [[[NSAttributedString alloc] initWithString:@"…"
                                        attributes:LineAttrs(font, color, kernPx)] autorelease];
    CTLineRef ellipsisLine = CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisAttr);
    CTLineRef truncated =
        CTLineCreateTruncatedLine(line, static_cast<double>(maxWidthPx), kCTLineTruncationEnd, ellipsisLine);
    if (ellipsisLine != nullptr) {
        CFRelease(ellipsisLine);
    }
    return truncated != nullptr ? truncated : (CTLineRef)CFRetain(line);
}

constexpr uint32_t kBitmapInfo =
    static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big);

}  // namespace

bool TextRasterizationAvailable() {
    return true;
}

int MeasureTextWidth(const TextRasterRequest& request) {
    if (request.utf8.empty()) {
        return 0;
    }
    @autoreleasepool {
        const double pixelSize = std::max(1.0, request.pointSize * request.scale);
        const double kernPx = std::max(0.0, request.tracking * request.scale);
        CTFontRef font = BorrowSystemFont(pixelSize, request.weight, request.family);
        CGColorRef color = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        CTLineRef line = CreateLine(SingleLine(request.utf8), font, color, kernPx);

        double width = 0.0;
        if (line != nullptr) {
            double ascent = 0.0;
            double descent = 0.0;
            double leading = 0.0;
            width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
            CFRelease(line);
        }
        CGColorRelease(color);
        return static_cast<int>(std::ceil(std::max(0.0, width)));
    }
}

bool RasterizeText(const TextRasterRequest& request, RasterizedText& out) {
    if (request.utf8.empty()) {
        return false;
    }

    @autoreleasepool {
        const double pixelSize = std::max(1.0, request.pointSize * request.scale);
        const double kernPx = std::max(0.0, request.tracking * request.scale);
        CTFontRef font = BorrowSystemFont(pixelSize, request.weight, request.family);
        CGColorRef color = CGColorCreateGenericRGB(
            static_cast<CGFloat>(request.r) / 255.0, static_cast<CGFloat>(request.g) / 255.0,
            static_cast<CGFloat>(request.b) / 255.0, 1.0);

        CTLineRef fullLine = CreateLine(SingleLine(request.utf8), font, color, kernPx);
        if (fullLine == nullptr) {
            CGColorRelease(color);
            return false;
        }
        CTLineRef line = TruncatedLine(fullLine, font, color, request.maxWidthPx, kernPx);
        CFRelease(fullLine);
        if (line == nullptr) {
            CGColorRelease(color);
            return false;
        }

        double ascent = 0.0;
        double descent = 0.0;
        double leading = 0.0;
        const double typoWidth = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

        // +2px de margen a la derecha para no cortar el antialiasing de
        // la última letra; ascent/descent ya cubren el recorte vertical.
        const int width = std::max(1, static_cast<int>(std::ceil(typoWidth)) + 2);
        const int height = std::max(1, static_cast<int>(std::ceil(ascent + descent)));
        const int baseline = std::clamp(static_cast<int>(std::ceil(ascent)), 0, height);

        const size_t bytesPerRow = static_cast<size_t>(width) * 4;
        const size_t byteCount = bytesPerRow * static_cast<size_t>(height);
        std::vector<std::uint8_t> premul(byteCount, 0);

        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            premul.data(), static_cast<size_t>(width), static_cast<size_t>(height), 8, bytesPerRow, rgb,
            kBitmapInfo);
        CGColorSpaceRelease(rgb);
        if (ctx == nullptr) {
            CFRelease(line);
            CGColorRelease(color);
            return false;
        }

        CGContextSetShouldAntialias(ctx, true);
        CGContextSetShouldSmoothFonts(ctx, false);
        // Medido en esta plataforma: el buffer de un CGBitmapContext
        // creado sobre memoria propia ya queda top-first (fila 0 EN
        // MEMORIA = borde superior de la imagen), así que NO se voltea al
        // copiar. Sin flip de CTM y con la baseline a `descent` del
        // borde inferior (= `ascent` del superior), los glyphs salen
        // derechos y en el orden correcto.
        CGContextSetTextPosition(ctx, 1.0, static_cast<CGFloat>(descent));
        CTLineDraw(line, ctx);
        CGContextFlush(ctx);

        out.width = width;
        out.height = height;
        out.baseline = baseline;
        out.pixels.assign(byteCount, 0);

        // Un-premultiplica, fila por fila, sin voltear.
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src = premul.data() + static_cast<size_t>(y) * bytesPerRow;
            std::uint8_t* dst = out.pixels.data() + static_cast<size_t>(y) * bytesPerRow;
            for (int x = 0; x < width; ++x) {
                const size_t p = static_cast<size_t>(x) * 4;
                const std::uint8_t a = src[p + 3];
                if (a == 0) {
                    continue;  // ya está en 0,0,0,0
                }
                const auto unpremul = [&](std::uint8_t c) -> std::uint8_t {
                    const int v = (static_cast<int>(c) * 255 + a / 2) / a;
                    return static_cast<std::uint8_t>(std::min(255, v));
                };
                dst[p + 0] = unpremul(src[p + 0]);
                dst[p + 1] = unpremul(src[p + 1]);
                dst[p + 2] = unpremul(src[p + 2]);
                dst[p + 3] = a;
            }
        }

        CGContextRelease(ctx);
        CFRelease(line);
        CGColorRelease(color);
        return true;
    }
}

}  // namespace nimvlets::platform
