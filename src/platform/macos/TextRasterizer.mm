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
CTFontRef BorrowSystemFont(double pixelSize, TextWeight weight) {
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

// CTLine creada (el caller la CFRelease-a). `color` es prestado.
CTLineRef CreateLine(NSString* text, CTFontRef font, CGColorRef color) {
    NSDictionary* attrs = @{
        (NSString*)kCTFontAttributeName : (id)font,
        (NSString*)kCTForegroundColorAttributeName : (id)color,
    };
    NSAttributedString* attributed =
        [[[NSAttributedString alloc] initWithString:text attributes:attrs] autorelease];
    return CTLineCreateWithAttributedString((CFAttributedStringRef)attributed);
}

// Devuelve una CTLine (el caller la CFRelease-a) recortada al final con
// "…" si `line` supera `maxWidthPx`; si entra o no se puede truncar,
// devuelve `line` con una referencia extra.
CTLineRef TruncatedLine(CTLineRef line, CTFontRef font, CGColorRef color, int maxWidthPx) {
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
                                        attributes:@{
                                            (NSString*)kCTFontAttributeName : (id)font,
                                            (NSString*)kCTForegroundColorAttributeName : (id)color,
                                        }] autorelease];
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
        CTFontRef font = BorrowSystemFont(pixelSize, request.weight);
        CGColorRef color = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        CTLineRef line = CreateLine(SingleLine(request.utf8), font, color);

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
        CTFontRef font = BorrowSystemFont(pixelSize, request.weight);
        CGColorRef color = CGColorCreateGenericRGB(
            static_cast<CGFloat>(request.r) / 255.0, static_cast<CGFloat>(request.g) / 255.0,
            static_cast<CGFloat>(request.b) / 255.0, 1.0);

        CTLineRef fullLine = CreateLine(SingleLine(request.utf8), font, color);
        if (fullLine == nullptr) {
            CGColorRelease(color);
            return false;
        }
        CTLineRef line = TruncatedLine(fullLine, font, color, request.maxWidthPx);
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
        // Sin flip de CTM: el origen de Core Graphics es abajo-izquierda,
        // el texto se dibuja "derecho", y la fila 0 EN MEMORIA queda
        // siendo la de ABAJO -> se copia invirtiendo filas más abajo.
        CGContextSetTextPosition(ctx, 1.0, static_cast<CGFloat>(descent));
        CTLineDraw(line, ctx);
        CGContextFlush(ctx);

        out.width = width;
        out.height = height;
        out.baseline = baseline;
        out.pixels.assign(byteCount, 0);

        // Un-premultiplica y voltea verticalmente en una sola pasada.
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src = premul.data() + static_cast<size_t>(height - 1 - y) * bytesPerRow;
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
