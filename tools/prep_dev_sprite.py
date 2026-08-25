#!/usr/bin/env python3
"""Dev-only asset prep: converts a source RGBA PNG into the tiny
uncompressed `.rgba` fixture format Block 01's spike loaded directly at
runtime (see AGENTS.md §10 and docs/DECISION_LOG.md). That format and
the `graphics::DevSprite` loader that read it have since been retired
(Block 02, see docs/DECISION_LOG.md DEC-023) — nothing in the runtime
loads `.rgba` anymore. `write_raw_rgba()`/`main()` below are kept as a
small, harmless historical utility, not because anything depends on
them now.

This module's real ongoing role is the shared, dependency-free PNG
read/write module reused by
Block 02's asset pipeline: `read_png_rgba()` is imported by
tools/compile_pet_pack.py (per AGENTS.md §10, "reuse, don't rewrite
working PNG tooling"), and `write_png_rgba()` is used by
tools/generate_bunny_dev_pack.py to materialize its deterministically
derived frames as real PNG files, so the pipeline documented in
docs/ANIMATION_RUNTIME.md is exercised with genuine PNG input end to
end, not an in-memory shortcut.

Block 04.2 adds `mirror_rgba_horizontal()`, reused by
tools/generate_nidir_pack.py to derive Nidir's "left" idle frames from
its real "right" idle frames by deterministic horizontal flip (never
AI-regenerated) -- see docs/NIDIR_CONTENT.md.

Block 04.2's second pass adds the generic display-size policy:
`compute_logical_canvas_size()` (a pet's on-screen logical size, derived
from its native art's aspect ratio, independent of that native
resolution) and `resize_rgba_nearest()` (the deterministic
nearest-neighbor resize `tools/compile_pet_pack.py` uses for the
optional `runtime_max_frame_dimension` compile-time downscale) -- see
docs/NIDIR_CONTENT.md, "tamaño de canvas lógico vs. resolución de
frame".

Block 04.3 agrega la política genérica de "canvas de trabajo
compartido, anclado por contenido": `compute_content_bbox()` (bounding
box real de pixeles visibles de un frame), `compose_on_canvas()`
(coloca un frame completo, sin recortar ni resamplear, dentro de un
canvas más grande y transparente) y `compute_frame_normalization_plan()`
(la política en sí -- deriva, a partir de los propios pixeles de cada
animación de un pet, un factor de escala y un desplazamiento por
entrada para que el personaje aparezca al mismo tamaño y en la misma
posición relativa sin importar qué animación esté activa, sin recortar
contenido en el proceso). Usado por `tools/compile_pet_pack.py` (el
campo de manifest opcional `normalize_visual_scale`) -- ver
docs/NIDIR_CONTENT.md, "clipping y tamaño visual inconsistente".

This file's own CLI entry point (`main()`, below) is a one-time,
offline prep step for temporary QA/dev fixtures — not a runtime tool,
not part of any content pipeline (see docs/PET_CONTENT_SPEC.md, still
unimplemented). Re-run it by hand if a source PNG changes.

Output format (all little-endian, no compression):
    magic:  4 bytes, ASCII "NVR1"
    width:  uint32
    height: uint32
    pixels: width * height * 4 bytes, RGBA8, row-major, top-to-bottom,
            straight (non-premultiplied) alpha.

Usage:
    python3 tools/prep_dev_sprite.py <input.png> <output.rgba>

No external dependencies (standard library only — struct + zlib).
"""

from __future__ import annotations

import math
import struct
import sys
import zlib


def read_png_rgba(path: str) -> tuple[int, int, bytes]:
    """Decodes a standard (non-interlaced, 8-bit RGBA) PNG into raw
    RGBA8 bytes. Deliberately minimal — this is a dev tool for one
    known-format source asset, not a general PNG library."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")

    pos = 8
    width = height = bitdepth = colortype = None
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"iDOT":
            raise ValueError(
                f"{path}: contains an Apple 'iDOT' fast-load chunk (typically from "
                "screencapture/Preview) that splits image data in a way this minimal "
                "decoder does not handle. Re-export via `sips -s format png` first "
                "to normalize it."
            )
        pos += 8 + length + 4
        if ctype == b"IEND":
            break

    if bitdepth != 8 or colortype != 6:
        raise ValueError(
            f"{path}: expected 8-bit RGBA (colortype 6), got bitdepth={bitdepth} "
            f"colortype={colortype}. Re-export as 8-bit RGBA PNG first."
        )

    raw = zlib.decompress(idat)
    bpp = 4
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        ftype = raw[p]
        p += 1
        line = bytearray(raw[p : p + stride])
        p += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 0xFF
            elif ftype == 2:
                line[x] = (line[x] + b) & 0xFF
            elif ftype == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out[y * stride : (y + 1) * stride] = line
        prev = line

    return width, height, bytes(out)


def mirror_rgba_horizontal(width: int, height: int, pixels: bytes) -> bytes:
    """Espeja horizontalmente un buffer RGBA8 (mismo layout que
    read_png_rgba/write_png_rgba: row-major, top-to-bottom, alpha
    directo) -- invierte el orden de columnas en cada fila, pixel por
    pixel completo (los 4 canales RGBA se mueven juntos, así que el
    canal alpha se preserva exactamente, nunca se recalcula ni se
    aproxima). Usado por tools/generate_nidir_pack.py (Block 04.2) para
    derivar los frames "left" a partir de los frames "right" reales de
    forma determinista, sin IA y sin ninguna otra transformación (ver
    block brief §3: "Do not use AI to regenerate the left side").

    Determinista y sin pérdida: espejar dos veces devuelve el buffer
    original byte a byte (ver tools/test_asset_pipeline.py)."""
    if len(pixels) != width * height * 4:
        raise ValueError(f"mirror_rgba_horizontal: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")

    stride = width * 4
    out = bytearray(len(pixels))
    for y in range(height):
        row_start = y * stride
        row = pixels[row_start : row_start + stride]
        for x in range(width):
            src_off = x * 4
            dst_x = width - 1 - x
            dst_off = dst_x * 4
            out[row_start + dst_off : row_start + dst_off + 4] = row[src_off : src_off + 4]
    return bytes(out)


# Tamaño "de referencia" del lado más largo del canvas lógico de un
# pet -- el mismo valor que Bunny ya usa desde Block 02
# (tools/generate_bunny_dev_pack.py's CANVAS_SIZE = 160), hecho
# explícito y reusable acá como la convención GENÉRICA de "tamaño de
# desktop companion" en vez de quedar implícito en un solo script.
REFERENCE_LOGICAL_SIZE = 160

# Factor de escala visual GLOBAL (Block 04.3). Se aplica DENTRO de
# compute_logical_canvas_size() -- automáticamente, para CUALQUIER pet
# que use esa función (Nidir y Bunny real), sin ninguna constante
# específica por pet y sin tocar ningún PNG fuente. **Para revertir al
# tamaño original: cambiar este único valor a 1.0 y volver a correr el
# generate_<pet>_pack.py de cada pet** -- ningún otro cambio de código
# hace falta.
#
# Historial (valor ABSOLUTO respecto al baseline original de 160, NUNCA
# multiplicativo/acumulado sobre el candidato anterior -- ver
# docs/NIDIR_CONTENT.md, "tamaño visual global"):
#   - Primera pasada de corrección: 1.05 (~+5% vs. baseline original,
#     el owner confirmó que le gustó).
#   - Segunda pasada de corrección (ESTE valor): 1.10 (~+10% vs.
#     baseline original -- el owner pidió otro +5% adicional partiendo
#     de 1.05, y el resultado debía quedar en +10% total respecto al
#     baseline, no en un +5% adicional compuesto sobre el +5% anterior,
#     es decir NO ~1.1025). Como este valor siempre se aplica contra el
#     REFERENCE_LOGICAL_SIZE original (160), no contra el candidato
#     previamente vigente, simplemente asignar 1.10 aquí ya produce el
#     resultado correcto sin ningún cálculo adicional.
DISPLAY_SIZE_SCALE_FACTOR = 1.10


def compute_logical_canvas_size(
    native_width: int, native_height: int, reference_size: int = REFERENCE_LOGICAL_SIZE, scale_factor: float | None = None
) -> tuple[int, int]:
    """Deriva el `canvas_width`/`canvas_height` LÓGICO de un pet (lo que
    ocupa en pantalla, en puntos -- ver src/app/SpikeApp.cpp,
    SDL_CreateWindow) a partir de la resolución NATIVA de su arte
    fuente, preservando el aspect ratio exacto: el lado más largo
    queda en `reference_size * scale_factor`, el otro se escala
    proporcionalmente (redondeo determinista al entero más cercano).

    `scale_factor`: si se omite (`None`), usa el valor GLOBAL actual
    de `DISPLAY_SIZE_SCALE_FACTOR` -- así cualquier llamador real
    (cualquier `generate_<pet>_pack.py`) recibe automáticamente la
    política de tamaño vigente sin tener que conocerla ni repetirla.
    Pasar un valor explícito (p. ej. `1.0`) omite el factor global --
    usado por los tests de esta función para verificar la matemática
    de aspect ratio en sí, independiente de qué candidato de tamaño
    esté vigente en un momento dado.

    Esto es intencionalmente independiente de la resolución de los PNG
    fuente -- el mismo `PetDefinition::canvasWidth/canvasHeight` que
    ya gobierna tanto el tamaño de renderizado (SDL_RenderTexture)
    como el del hit-mask (core::AlphaMask::FromAlphaChannel) desde
    Block 02, ambos ya escalando desde CUALQUIER resolución nativa
    hacia el canvas -- así que ningún cambio de runtime hizo falta
    para esto, solo elegir el VALOR correcto en vez de copiar la
    resolución nativa 1:1 (ver docs/NIDIR_CONTENT.md, "tamaño de
    canvas lógico vs. resolución de frame", para el bug que esto
    corrige en el Nidir de la primera pasada de este bloque).

    Genérico -- no tiene ninguna rama por pet: cualquier pet futuro
    con cualquier resolución/aspect ratio nativa obtiene un canvas
    lógico "clase 160" (o "clase 160 × factor vigente") comparable,
    calculado con la misma fórmula."""
    if native_width <= 0 or native_height <= 0:
        raise ValueError(f"compute_logical_canvas_size: dimensiones nativas inválidas {native_width}x{native_height}")

    effective_scale_factor = DISPLAY_SIZE_SCALE_FACTOR if scale_factor is None else scale_factor
    effective_reference_size = reference_size * effective_scale_factor

    longer = max(native_width, native_height)
    scale = effective_reference_size / longer
    canvas_width = max(1, round(native_width * scale))
    canvas_height = max(1, round(native_height * scale))
    return canvas_width, canvas_height


def resize_rgba_nearest(width: int, height: int, pixels: bytes, target_width: int, target_height: int) -> bytes:
    """Reescala un buffer RGBA8 a `target_width`x`target_height` por
    muestreo nearest-neighbor INVERSO (destino -> fuente) -- la misma
    fórmula de mapeo que core::AlphaMask::FromAlphaChannel y
    tools/generate_bunny_dev_pack.py's resize_nearest() ya usan,
    mantenida deliberadamente consistente entre el runtime C++ y el
    tooling Python (ver docs/ANIMATION_RUNTIME.md). Usado por
    tools/compile_pet_pack.py para el downscale opcional en tiempo de
    compilación (`runtime_max_frame_dimension`) -- nunca modifica los
    PNG fuente en disco, solo los bytes que terminan adentro del pack
    compilado."""
    if len(pixels) != width * height * 4:
        raise ValueError(f"resize_rgba_nearest: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")
    if target_width <= 0 or target_height <= 0:
        raise ValueError(f"resize_rgba_nearest: target dimensions must be positive, got {target_width}x{target_height}")

    src_stride = width * 4
    out = bytearray(target_width * target_height * 4)
    for ty in range(target_height):
        sy = min(height - 1, (ty * height) // target_height)
        src_row = sy * src_stride
        for tx in range(target_width):
            sx = min(width - 1, (tx * width) // target_width)
            src_off = src_row + sx * 4
            dst_off = (ty * target_width + tx) * 4
            out[dst_off : dst_off + 4] = pixels[src_off : src_off + 4]
    return bytes(out)


def resize_rgba_area_average(width: int, height: int, pixels: bytes, target_width: int, target_height: int) -> bytes:
    """Downscale de calidad, determinista (box filter -- promedia todos
    los pixeles fuente que caen dentro del área de cada pixel destino,
    misma fórmula de mapeo proporcional destino->fuente que
    `resize_rgba_nearest`, solo que sobre un RANGO en vez de un único
    punto muestra). Agregado en Block 04.3 (corrección post-QA) para
    reemplazar `resize_rgba_nearest` específicamente en los pasos de
    compilación que son downscales reales (`runtime_max_frame_dimension`,
    y el reescalado de `content_scale` cuando reduce) -- nearest-neighbor
    en un downscale real puede, por mala suerte de en qué punto cae la
    muestra, saltarse por completo detalles finos (p. ej. un contorno
    delgado de 1-2px) que SÍ existen en el frame fuente, produciendo el
    efecto de "pixeles que desaparecen" que QA manual reportó para
    Bunny -- un box filter que promedia TODO el área, en cambio, nunca
    ignora contenido real por completo, solo lo difumina levemente.

    RGB se promedia PONDERADO por alpha (no un promedio simple) --
    necesario porque los pixeles completamente transparentes pueden
    tener valores RGB arbitrarios/ruidosos sin ningún efecto visual
    (ver docs/NIDIR_CONTENT.md, "first/last frame contract"); promediar
    RGB sin ponderar dejaría que ese ruido contamine el color de los
    pixeles visibles vecinos al reducir la imagen (un artefacto de
    "fringing" de color en los bordes). Alpha se promedia sin ponderar
    (es la propia cantidad que decide el peso de todo lo demás).

    Determinista: mismo resultado siempre para la misma entrada. NO
    reemplaza a `resize_rgba_nearest` en general -- esa sigue siendo la
    función correcta para upscales (donde un box filter degenera) y
    para cualquier caso que deba coincidir exactamente con el muestreo
    nearest-neighbor de `core::AlphaMask::FromAlphaChannel` en runtime
    (esta función es solo para compilar bytes de PIXELES de textura,
    nunca para el hit-mask, que sigue sin cambios)."""
    if len(pixels) != width * height * 4:
        raise ValueError(
            f"resize_rgba_area_average: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8"
        )
    if target_width <= 0 or target_height <= 0:
        raise ValueError(f"resize_rgba_area_average: target dimensions must be positive, got {target_width}x{target_height}")

    src_stride = width * 4
    out = bytearray(target_width * target_height * 4)
    for ty in range(target_height):
        y0 = (ty * height) // target_height
        y1 = max(y0 + 1, ((ty + 1) * height) // target_height)
        y1 = min(y1, height)
        for tx in range(target_width):
            x0 = (tx * width) // target_width
            x1 = max(x0 + 1, ((tx + 1) * width) // target_width)
            x1 = min(x1, width)

            r_sum = g_sum = b_sum = 0
            a_sum = 0
            alpha_weight_sum = 0
            sample_count = 0
            for sy in range(y0, y1):
                row_off = sy * src_stride
                for sx in range(x0, x1):
                    off = row_off + sx * 4
                    a = pixels[off + 3]
                    r_sum += pixels[off] * a
                    g_sum += pixels[off + 1] * a
                    b_sum += pixels[off + 2] * a
                    a_sum += a
                    alpha_weight_sum += a
                    sample_count += 1

            dst_off = (ty * target_width + tx) * 4
            if alpha_weight_sum > 0:
                out[dst_off] = round(r_sum / alpha_weight_sum)
                out[dst_off + 1] = round(g_sum / alpha_weight_sum)
                out[dst_off + 2] = round(b_sum / alpha_weight_sum)
            # si no, el área es completamente transparente -- RGB queda
            # en 0 (todo-ceros por defecto), sin efecto visual.
            out[dst_off + 3] = round(a_sum / sample_count) if sample_count > 0 else 0
    return bytes(out)


# Umbral bajo por defecto para el bounding box de CONTENIDO visible --
# deliberadamente distinto del umbral de hit-testing de clicks (128,
# ver DEC-018), que decide qué cuenta como "clickeable". Acá se busca
# la extensión visual REAL de un frame (para políticas de encuadre/
# tamaño), donde incluso un borde antialiaseado muy tenue debe contar
# como "hay algo ahí" -- un umbral alto subestimaría cuánto espacio
# ocupa realmente el contenido.
DEFAULT_CONTENT_ALPHA_THRESHOLD = 8


def compute_content_bbox(
    width: int, height: int, pixels: bytes, alpha_threshold: int = DEFAULT_CONTENT_ALPHA_THRESHOLD
) -> tuple[int, int, int, int] | None:
    """Bounding box (min_x, min_y, max_x, max_y), inclusive, de los
    pixeles con alpha > `alpha_threshold`. Devuelve None si el frame es
    completamente transparente (no hay ningún pixel por encima del
    umbral) -- el llamador decide qué hacer en ese caso, esta función
    nunca inventa un bounding box para contenido que no existe."""
    if len(pixels) != width * height * 4:
        raise ValueError(f"compute_content_bbox: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")

    min_x = min_y = None
    max_x = max_y = -1
    for y in range(height):
        row = y * width * 4
        for x in range(width):
            if pixels[row + x * 4 + 3] > alpha_threshold:
                if min_x is None or x < min_x:
                    min_x = x
                if min_y is None or y < min_y:
                    min_y = y
                if x > max_x:
                    max_x = x
                if y > max_y:
                    max_y = y
    if min_x is None:
        return None
    return min_x, min_y, max_x, max_y


def compose_on_canvas(
    width: int, height: int, pixels: bytes, canvas_width: int, canvas_height: int, offset_x: int, offset_y: int
) -> bytes:
    """Coloca el frame COMPLETO (sin recortar ni resamplear) dentro de
    un canvas más grande y transparente, en la posición entera
    (offset_x, offset_y) -- una copia directa de pixeles, nunca un
    resample, así que preserva el contenido exactamente. El llamador es
    responsable de que el canvas de destino sea lo bastante grande como
    para contener el frame completo en esa posición
    (compute_frame_normalization_plan() ya lo garantiza); si no lo
    fuera, esta función recorta silenciosamente lo que no entra en vez
    de fallar, documentado acá, no una sorpresa."""
    if len(pixels) != width * height * 4:
        raise ValueError(f"compose_on_canvas: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")
    if canvas_width <= 0 or canvas_height <= 0:
        raise ValueError(f"compose_on_canvas: canvas dimensions must be positive, got {canvas_width}x{canvas_height}")

    out = bytearray(canvas_width * canvas_height * 4)  # todo-ceros == completamente transparente
    x0 = max(0, -offset_x)
    x1 = min(width, canvas_width - offset_x)
    if x1 <= x0:
        return bytes(out)  # el frame cae completamente fuera del canvas en X

    for y in range(height):
        dst_y = offset_y + y
        if dst_y < 0 or dst_y >= canvas_height:
            continue
        src_row_off = y * width * 4
        src_start = src_row_off + x0 * 4
        src_end = src_row_off + x1 * 4
        dst_start = (dst_y * canvas_width + (offset_x + x0)) * 4
        out[dst_start : dst_start + (src_end - src_start)] = pixels[src_start:src_end]
    return bytes(out)


def alpha_weighted_centroid(width: int, height: int, pixels: bytes) -> tuple[float, float] | None:
    """Centroide (cx, cy) ponderado por alpha de un frame RGBA8 --
    "dónde está el CENTRO DE MASA de lo visible", a diferencia del
    centro geométrico de su bounding box (que solo mira los pixeles
    EXTREMOS). Extraído de `alpha_rms_radius()` (Block 05, pasada de
    estabilización) para reusarlo también en el registro de PUNTOS DE
    ANCLAJE de `compute_frame_normalization_plan()` (Block 05, pasada
    de pulido final -- ver docs/DECISION_LOG.md DEC-093 para la
    evidencia real de por qué el centro de bbox no alcanza ahí).

    Devuelve None para un frame completamente transparente -- el
    llamador decide qué hacer (nunca se inventa un centro para
    contenido que no existe)."""
    if len(pixels) != width * height * 4:
        raise ValueError(
            f"alpha_weighted_centroid: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8"
        )
    total = 0
    sum_x = 0
    sum_y = 0
    for y in range(height):
        row = y * width * 4
        for x in range(width):
            a = pixels[row + x * 4 + 3]
            if a:
                total += a
                sum_x += x * a
                sum_y += y * a
    if total == 0:
        return None
    return sum_x / total, sum_y / total


def alpha_rms_radius(width: int, height: int, pixels: bytes) -> float:
    """Radio RMS ponderado por alpha alrededor del centroide de alpha —
    el estimador de "qué tan grande es este personaje" que usa
    compute_frame_normalization_plan().

    Por qué NO el lado más largo del bounding box (Block 05, pasada de
    estabilización — ver docs/DECISION_LOG.md DEC-088): el lado más
    largo se decide con DOS pixeles extremos, así que una pose autorada
    que estira una oreja, una cola o un hocico cambia la "medida" del
    personaje sin que su tamaño haya cambiado en absoluto. Medido en el
    contenido real de este repo: el `howl` de Frin macho tiene el bbox
    +4.5% más ANCHO que la pose base sentada con EXACTAMENTE la misma
    altura — no es escala, es la silueta autorada (la cabeza sube y la
    cola se abre). Un correctivo calculado sobre ancho de bbox
    aplastaría esa pose real.

    El radio RMS integra TODOS los pixeles con su alpha como peso, así
    que escala linealmente con un reescalado uniforme (que es lo que
    queremos medir) y apenas se mueve con un cambio de pose (que no lo
    es). Es la "alpha-registration" que este bloque pidió explícitamente
    en vez de "raw bbox width alone".

    Es invariante a traslación (se mide contra el centroide propio), así
    que dos frames del mismo personaje en distinta posición dentro de su
    frame nativo dan el mismo valor.

    Devuelve 0.0 para un frame completamente transparente — el llamador
    ya trata ese caso (nunca se divide por él sin chequear)."""
    centroid = alpha_weighted_centroid(width, height, pixels)
    if centroid is None:
        return 0.0
    cx, cy = centroid

    total = 0
    variance = 0.0
    for y in range(height):
        row = y * width * 4
        dy2 = (y - cy) ** 2
        for x in range(width):
            a = pixels[row + x * 4 + 3]
            if a:
                total += a
                variance += a * ((x - cx) ** 2 + dy2)
    return math.sqrt(variance / total)


def bbox_registration_point(width: int, height: int, pixels: bytes, scale: float) -> tuple[float, float]:
    """Centro del bounding box de contenido de un frame, YA escalado --
    el punto de anclaje que `compute_frame_normalization_plan()` usaba
    exclusivamente hasta DEC-093 (y que sigue usando para la colocación
    DEFAULT de cualquier entrada sin destino de transición registrado).
    Extraído a nivel de módulo (Block 05, pasada de continuidad de
    frontera) para poder reusarlo desde
    `compute_two_endpoint_frame_offsets()` sin duplicar la lógica de
    convenio de pixel-a-centro (+1 sobre el bbox inclusivo, dividido por
    2 -- un pixel de índice x cubre [x, x+1))."""
    minx, miny, maxx, maxy = content_bbox_or_full_frame(width, height, pixels)
    return ((minx + maxx + 1) / 2.0 * scale, (miny + maxy + 1) / 2.0 * scale)


def alpha_registration_point(width: int, height: int, pixels: bytes, scale: float) -> tuple[float, float]:
    """Centroide ponderado por alpha de un frame, YA escalado -- el
    punto de anclaje que `compute_frame_normalization_plan()` usa desde
    DEC-093 para registrar una punta de transición contra su destino
    (ver `alpha_weighted_centroid()`/DEC-093 para por qué centroide de
    alpha y no centro de bbox). Cae a `bbox_registration_point()` si el
    frame es completamente transparente -- mismo convenio pixel-índice
    -> pixel-centro (+0.5) que ya documentaba la versión anidada
    original."""
    centroid = alpha_weighted_centroid(width, height, pixels)
    if centroid is None:
        return bbox_registration_point(width, height, pixels, scale)
    cx, cy = centroid
    return (cx + 0.5) * scale, (cy + 0.5) * scale


def content_bbox_or_full_frame(width: int, height: int, pixels: bytes) -> tuple[int, int, int, int]:
    """Bounding box de contenido (`compute_content_bbox()`), o el frame
    COMPLETO si no hay ningún pixel por encima del umbral -- nunca se
    inventa contenido que no existe; el frame entero es el único
    fallback razonable para un frame totalmente transparente. Extraído
    a nivel de módulo (antes vivía anidado dentro de
    `compute_frame_normalization_plan()`) para que
    `bbox_registration_point()` pueda reusarlo fuera de esa función."""
    bbox = compute_content_bbox(width, height, pixels)
    if bbox is not None:
        return bbox
    return 0, 0, width - 1, height - 1


# NOTA (pasada de resolución de root-motion): acá vivían
# `compute_two_endpoint_frame_offsets()` y `lerp_offset_schedule()` --
# el mecanismo de registro de DOS PUNTAS con interpolación LINEAL de
# traslación por índice de frame que la pasada anterior introdujo para
# el `lie_to_sit` de Frin. **QA manual del owner lo RECHAZÓ**: mover la
# colocación del sprite gradualmente a lo largo del clip no se percibe
# como "continuidad", se percibe como el personaje ENTERO derivando por
# la ventana mientras se levanta -- root-motion artificial que el arte
# nunca autoró. Ver docs/DECISION_LOG.md DEC-097.
#
# Se eliminaron enteros en vez de dejarlos "por si acaso": el
# contrato ahora es que el compilador NUNCA inventa movimiento aparente
# del personaje completo. Una transición recibe UNA sola transforma
# rígida (una escala uniforme + una traslación constante) para todo el
# clip, y punto. Si las dos puntas no cierran con eso, el residual se
# MIDE y se reporta -- no se disimula con movimiento falso.


def compute_frame_normalization_plan(
    entries: dict[str, tuple[int, int, bytes]],
    groups: dict[str, str],
    reference_group: str,
    group_frame_paths: dict[str, list[str]],
    state_of_group: dict[str, str],
    base_group_of_state: dict[str, str],
    entry_frame_paths: dict[str, list[str]] | None = None,
    last_frames: dict[str, tuple[int, int, bytes]] | None = None,
    transition_target_entry: dict[str, str] | None = None,
    start_anchor_entry: dict[str, str] | None = None,
    scale_from_last_frame_entries: "set[str] | None" = None,
    strict_scale_entries: "set[str] | None" = None,
    scale_tolerance: float = 0.005,
) -> dict[str, tuple[float, int, int, int, int]]:
    """Política genérica de "canvas de trabajo compartido, anclado por
    contenido" (Block 04.3 -- ver docs/NIDIR_CONTENT.md, "clipping y
    tamaño visual inconsistente entre animaciones"), extendida en Block
    05 (segunda pasada de corrección post-QA) con una TRANSFORMA
    CANÓNICA POR ESTADO -- ver docs/DECISION_LOG.md DEC-075. Corrige el
    bug de raíz encontrado en QA manual de Nidir: cada frame se
    estiraba independientemente para llenar el mismo canvas lógico fijo
    (SpikeApp::RenderFrame(), sin cambios -- sigue siendo un simple
    stretch-to-fill), así que dos animaciones con distinta cantidad de
    margen alrededor del personaje dentro de su propio frame nativo
    (p. ej. el click-fire de Nidir, que necesita espacio extra para el
    efecto de fuego) terminaban mostrando al personaje a un tamaño y
    posición distintos en pantalla, sin que el código de render tuviera
    ningún bug propio -- el problema era que cada animación se
    escalaba/encuadraba de forma independiente en vez de compartir un
    marco de referencia común.

    `entries`: cada "entrada compilable" (una animación canónica o un
    override direccional) mapeada a los pixels de su PRIMER frame
    (nativo, sin ningún downscale todavía) -- el primer frame es, por
    convención ya establecida en este proyecto (ver "first/last frame
    contract", Block 04.2), la pose base/de-reposo, la referencia más
    estable para anclar el resto de la secuencia.
    `groups`: entry_key -> nombre de grupo lógico ("idle",
    "click_reaction", "state[lying].click_actions.lie_to_sit", ...) --
    una animación canónica y sus overrides direccionales SIEMPRE
    comparten grupo, así que right/left de una misma animación
    terminan con idéntico content_scale (nunca escalas distintas para
    las dos direcciones de la misma animación).
    `reference_group`: el grupo del `base_animation` del PRIMER estado
    del pet -- ancla absoluta de escala "1.0" (ver DEC-045: idle/la
    pose base ya era la referencia para el tamaño de canvas lógico;
    acá se reusa la misma convención para el tamaño de CONTENIDO).
    `group_frame_paths`: group_key -> lista de rutas ABSOLUTAS de TODOS
    los frames (no solo el primero) de TODAS las entradas de ese grupo
    -- usada para detectar cuándo dos grupos distintos en realidad
    comparten un archivo fuente literal (ver más abajo).
    `state_of_group`: group_key -> id del `BehaviorState` que autoriza
    ese grupo (bajo qué estado vive en el manifest).
    `base_group_of_state`: state_id -> group_key del `base_animation`
    de ESE estado específicamente (nunca un override direccional).

    **Por qué un estado no puede compararse contra otro por bounding
    box (evidencia real, Block 05 segunda pasada):** medir el "tamaño"
    de un grupo como el lado más largo de su bounding box de contenido
    asume que todos los grupos comparten la MISMA orientación de
    silueta. Eso es razonablemente cierto para animaciones del mismo
    estado (todas parten de la misma pose de reposo, solo con distinta
    cantidad de margen nativo), pero es FALSO entre dos
    `BehaviorState`s con posturas genuinamente distintas -- Frin
    "seated" (alto y angosto) vs. "lying" (bajo y ancho): el lado más
    largo de "seated" es la altura, el de "lying" es el ancho, así que
    compararlos por "lado más largo" compara ejes distintos y produce
    un content_scale sin sentido (medido en este bloque: 1.31x-1.52x de
    inflación real en Frin, confirmado con capturas del binario
    corriendo). La solución NO es una mejor fórmula de bounding box --
    es no comparar por bounding box en absoluto cuando dos entradas ya
    están vinculadas por CONTENIDO REAL: si `state[lying].base_animation`
    literalmente REFERENCIA el mismo archivo que el frame final de
    `sit_to_lie` (el contrato first/last-frame que este proyecto ya
    exige -- ver docs/NIDIR_CONTENT.md), entonces "lying" y "seated" NO
    son dos mediciones independientes que haya que reconciliar: son EL
    MISMO personaje en el mismo frame, así que deben compartir
    content_scale POR CONSTRUCCIÓN, nunca por una nueva comparación de
    pixeles. Este `group_frame_paths`-based union-find hace exactamente
    eso -- dos grupos que comparten CUALQUIER archivo de frame (no solo
    el primero) se fusionan en un único "scale_group" con una única
    escala resuelta.

    Para una acción que SÍ pertenece a un estado sin vínculo de archivo
    con la referencia (p. ej. `lie_to_sit`, un export de reversa
    genuinamente distinto de `sit_to_lie`), la comparación válida NO es
    contra el estado de referencia del pet (otra orientación) sino
    contra el `base_animation` de SU PROPIO estado ("lying"), que ya
    comparte silueta/orientación por ser la MISMA postura -- eso es lo
    que `base_group_of_state`/`state_of_group` permiten expresar
    genéricamente, sin ninguna rama de código específica de Frin ni de
    ningún otro pet: cualquier pet futuro con estados reales que siga
    el mismo contrato first/last-frame obtiene esta corrección gratis.

    Para cada entrada calcula:
    - content_scale (compartido por grupo, luego por scale_group vía
      el union-find de arriba): factor de reescalado para que el
      contenido visible (bounding box de alpha) de este grupo ocupe el
      mismo tamaño absoluto en pixeles que el de su ANCLA (el
      scale_group de referencia si está transitivamente vinculado por
      archivo, si no el `base_animation` de su propio estado). Si la
      diferencia entra dentro de `scale_tolerance` (2% por defecto), se
      usa 1.0 -- evita un resample innecesario que degradaría calidad
      sin corregir nada perceptible.
    - Un canvas de trabajo COMPARTIDO por TODO el pet (mismas
      dimensiones para todas las entradas, de todos los grupos/estados
      -- así un pet con estados nunca "salta" de tamaño de canvas al
      transicionar), lo bastante grande como para contener cada frame
      completo (post-content_scale) sin recortar nada, calculado
      alineando el CENTRO del bounding box de contenido del primer
      frame de cada entrada al centro del canvas de trabajo.
    - offset_x/offset_y: dónde colocar el frame (post-content_scale,
      sin recortar) dentro de ese canvas de trabajo compartido, para
      que su ancla de contenido caiga exactamente en el centro.

    `transition_target_entry`/`last_frames` (opcionales, Block 05 --
    ver DEC-087 y, para su extensión a self-loop + el cambio de punto
    de anclaje, DEC-093): cuando una entrada tiene un destino
    registrado (una transición que cambia de estado, o una acción
    self-loop con `align_endpoint_to_target_base` en el manifest --
    ver tools/compile_pet_pack.py), su colocación se ancla por su
    ÚLTIMO frame contra la pose base de destino en vez de por su propio
    frame 0 -- salvo que ya esté vinculada por archivo compartido
    (containment, que es exacto por construcción y tiene prioridad).
    Ese anclaje usa el CENTROIDE PONDERADO POR ALPHA de cada punta
    (`registration_point()`, no `anchor_of()`) -- medido en este bloque
    que anclar por centro de bounding box puede alinear los pixeles
    EXTREMOS perfectamente mientras el centro de MASA real queda peor
    que sin ningún anclaje, para una pose con margen asimétrico. Fuera
    de esta rama (colocación DEFAULT de cualquier entrada sin destino
    registrado, y la posición del propio destino) sigue siendo
    exclusivamente centro-de-bbox, sin cambios.

    `scale_from_last_frame_entries` (opcional, pasada de continuidad de
    frontera -- ver `align_endpoint_to_target_base` en
    tools/compile_pet_pack.py y docs/DECISION_LOG.md): conjunto de
    entry_keys de acciones SELF-LOOP (`target_state_id == state_id`,
    nunca una transición que cambia de estado -- ver más abajo por
    qué) cuya ESCALA (no solo su colocación) debe derivarse del último
    frame (el que REALMENTE toca la base cuando la acción termina) en
    vez del primero. Sin esto, `group_content_size()` mide SIEMPRE el
    primer frame -- válido para "¿qué tan grande es la pose de
    ARRANQUE de esta acción?", pero no necesariamente para "¿qué tan
    grande es la pose de REGRESO?", que es la que un `content_scale`
    uniforme deja tocando (o no) la base sin ningún salto de tamaño
    perceptible. Solo aplica a self-loop porque ahí frame 0 Y el
    último frame comparten la MISMA postura que la base (sentado todo
    el tiempo, o "default" todo el tiempo) -- para una transición que
    SÍ cambia de postura (`lie_to_sit`: empieza acostado, termina
    sentado) comparar el ÚLTIMO frame (postura sentada) contra la base
    de ORIGEN (postura acostada) sería la misma comparación
    entre-posturas inválida que DEC-075 ya prohibió -- ahí la escala
    sigue derivándose del primer frame contra la base de origen, sin
    cambios (ver `start_anchor_entry` más abajo para la COLOCACIÓN de
    esa misma transición, que es un problema aparte).

    `strict_scale_entries` (opcional, pasada de resolución de
    root-motion -- ver DEC-098): conjunto de entry_keys cuya escala se
    resuelve SIN aplicar `scale_tolerance`. Es exactamente el mismo
    conjunto que `scale_from_last_frame_entries` en la práctica (una
    acción que declara `align_endpoint_to_target_base` quiere las dos
    cosas: medir desde el último frame Y registrar ese tamaño exacto),
    pero se pasa por separado para que las dos políticas sigan siendo
    independientes en el nivel de la función. La tolerancia normal
    existe para no resamplear cuando la diferencia es imperceptible EN
    AISLAMIENTO; en la frontera de retorno, donde el owner ve dos
    imágenes consecutivas del MISMO personaje, un 0.1-0.25% sí se
    percibe (QA manual real). Ver el comentario en el bucle de escala.

    `start_anchor_entry` (opcional, pasada de resolución de
    root-motion -- ver DEC-097): entry_key -> entry_key de la base del
    estado de ORIGEN. Ancla la entrada por su PRIMER frame contra esa
    base, con UNA transforma rígida constante para todo el clip, y
    REEMPLAZA (nunca complementa) el anclaje por-último-frame de
    `transition_target_entry`. Existe porque un salto instantáneo en el
    PRIMER frame -- el instante exacto en que el owner hace click -- es
    mucho más notorio que un residual al final, cuando el personaje ya
    viene en movimiento.

    Deliberadamente NO existe un modo "anclar las dos puntas a la vez":
    cuando el export no cierra geométricamente, satisfacer las dos
    exigiría mover el sprite durante el clip, y eso se probó y se
    RECHAZÓ en QA (se percibe como root-motion artificial, no como
    continuidad -- ver DEC-097). Si las dos puntas no cierran con una
    sola transforma rígida, el residual se MIDE y se reporta como deuda
    de CONTENIDO; el compilador no lo disimula.

    Nunca recorta contenido -- solo agrega margen transparente
    (compose_on_canvas() nunca resamplea). Sin ninguna rama específica
    de personaje: cuánto escalar cada grupo y qué tan grande debe ser
    el canvas de trabajo se derivan enteramente de los pixeles reales
    y de la estructura del grafo de estados (nunca de un valor
    hardcodeado por pet) -- reusable tal cual para cualquier Nimvlet
    futuro, con o sin estados, con animaciones de distinto encuadre
    nativo."""
    if reference_group not in groups.values():
        raise ValueError(f"compute_frame_normalization_plan: reference_group '{reference_group}' is not used by any entry")

    # `content_bbox_or_full_frame()` vive ahora a nivel de módulo (ver
    # más arriba) -- se reusa tal cual desde acá, sin redefinirla.

    canonical_of_group: dict[str, str] = {}
    for entry_key, group_key in groups.items():
        canonical_of_group.setdefault(group_key, entry_key)

    scale_from_last = scale_from_last_frame_entries or set()
    strict_scale = strict_scale_entries or set()
    last_frame_pixels_for_scale = last_frames or {}

    def group_content_size(group_key: str) -> float:
        entry_key = canonical_of_group[group_key]
        # Self-loop con escala derivada del RETORNO (ver el docstring
        # de `scale_from_last_frame_entries` más arriba): mide el
        # ÚLTIMO frame, no el primero, cuando el contenido lo pide
        # explícitamente. `last_frame_pixels_for_scale.get(entry_key)`
        # puede faltar si esta entrada terminó excluida de
        # `last_frames` por compartir archivo con su destino
        # (containment ya la deja exacta) -- en ese caso cae al frame 0
        # como siempre, correcto porque containment ya garantiza
        # escala 1.0 por construcción.
        if entry_key in scale_from_last and entry_key in last_frame_pixels_for_scale:
            w, h, pixels = last_frame_pixels_for_scale[entry_key]
        else:
            w, h, pixels = entries[entry_key]
        return alpha_rms_radius(w, h, pixels)

    # --- Union-Find de grupos vinculados por archivo REAL compartido
    # (cualquier frame, no solo el primero) -- ver el docstring de
    # arriba. Dos grupos que nunca comparten ningún archivo permanecen
    # en su propio cluster de un solo elemento, sin cambio de
    # comportamiento respecto a antes de Block 05.
    scale_group_parent: dict[str, str] = {g: g for g in group_frame_paths}

    def find_scale_group(g: str) -> str:
        while scale_group_parent[g] != g:
            scale_group_parent[g] = scale_group_parent[scale_group_parent[g]]
            g = scale_group_parent[g]
        return g

    def union_scale_groups(a: str, b: str) -> None:
        ra, rb = find_scale_group(a), find_scale_group(b)
        if ra != rb:
            scale_group_parent[rb] = ra

    path_owner: dict[str, str] = {}
    for group_key, paths in group_frame_paths.items():
        for path in paths:
            owner = path_owner.get(path)
            if owner is None:
                path_owner[path] = group_key
            elif find_scale_group(owner) != find_scale_group(group_key):
                union_scale_groups(owner, group_key)

    scale_group_of: dict[str, str] = {g: find_scale_group(g) for g in group_frame_paths}
    reference_scale_group = scale_group_of[reference_group]

    # --- Resuelve, por ESTADO (no por grupo/scale_group directamente),
    # el factor de escala de su PROPIO base_animation -- memoizado
    # porque varios grupos de un mismo estado comparten esta misma
    # resolución.
    resolved_scale_group_value: dict[str, float] = {reference_scale_group: 1.0}

    def resolve_state_base_scale(state_id: str) -> float:
        base_group = base_group_of_state[state_id]
        sg = scale_group_of[base_group]
        if sg in resolved_scale_group_value:
            return resolved_scale_group_value[sg]
        # Único caso restante que SÍ compara bounding boxes entre
        # estados potencialmente distintos -- solo se alcanza si el
        # base_animation de este estado NUNCA quedó vinculado por
        # archivo real al estado de referencia (un pet futuro cuyo
        # contenido no siga el contrato first/last-frame entre
        # estados). Documentado como límite honesto, no oculto: ver el
        # informe de este bloque.
        this_size = group_content_size(base_group)
        ref_size = group_content_size(reference_group)
        raw_scale = ref_size / this_size if this_size > 0 else 1.0
        resolved = 1.0 if abs(raw_scale - 1.0) <= scale_tolerance else raw_scale
        resolved_scale_group_value[sg] = resolved
        return resolved

    scale_by_group: dict[str, float] = {}
    for group_key in set(groups.values()):
        state_id = state_of_group[group_key]
        base_group = base_group_of_state[state_id]
        state_scale = resolve_state_base_scale(state_id)
        if group_key == base_group:
            # El propio base_animation de un estado -- su escala ES la
            # escala resuelta de su estado, sin ninguna comparación
            # adicional (evita recompararlo contra sí mismo).
            scale_by_group[group_key] = state_scale
            continue
        this_size = group_content_size(group_key)
        base_size = group_content_size(base_group)
        raw_scale = base_size / this_size if this_size > 0 else 1.0
        # Contrato ESTRICTO de retorno-a-base (pasada de resolución de
        # root-motion -- ver docs/DECISION_LOG.md DEC-098): para una
        # acción que el CONTENIDO marca explícitamente como
        # "mi último frame representa la pose base del estado destino"
        # (`align_endpoint_to_target_base`), la tolerancia normal de
        # `scale_tolerance` NO aplica -- se usa `raw_scale` tal cual.
        #
        # Por qué: esa tolerancia existe para evitar un resample
        # innecesario cuando la diferencia es imperceptible EN GENERAL,
        # y es la política correcta para cualquier animación normal.
        # Pero en la FRONTERA de retorno el owner compara dos imágenes
        # consecutivas del MISMO personaje, una al lado de la otra en
        # el tiempo -- ahí una diferencia de 0.1-0.25% que sería
        # invisible en aislamiento se percibe como "el personaje se
        # achicó al terminar la animación" (QA manual real). Medido:
        # sin esta rama, `idle_breathing` de Bunny (1.00112 requerido)
        # y `howl` de Frin hembra (0.99779 requerido) quedaban ambos
        # ajustados a 1.0 exacto por la tolerancia.
        #
        # El costo es un resample que de otro modo no ocurriría -- se
        # acepta deliberadamente acá y SOLO acá: es opt-in por-acción
        # desde el contenido, nunca global, así que ninguna animación
        # que no declare este contrato paga nada.
        if canonical_of_group.get(group_key) in strict_scale:
            local_scale = raw_scale
        else:
            local_scale = 1.0 if abs(raw_scale - 1.0) <= scale_tolerance else raw_scale
        scale_by_group[group_key] = state_scale * local_scale

    # --- Colocación (offset) ------------------------------------------
    #
    # `anchor(E)` = centro del bounding box de contenido del FRAME DE
    # REGISTRO de E, ya escalado. `pos(E)` = coordenada flotante de la
    # esquina superior-izquierda de E en un marco provisional donde el
    # ancla compartida está en el origen; al final se traslada todo para
    # que nada quede negativo y de ahí sale el canvas de trabajo.
    #
    # Sin containment ni transiciones de estado esto es EXACTAMENTE la
    # aritmética anterior (pos = -anchor), solo reescrita: cada entrada
    # pone el centro de contenido de su primer frame en el mismo punto.
    entry_paths = entry_frame_paths or {}
    start_anchors = start_anchor_entry or {}
    last_frame_pixels = last_frames or {}
    transition_targets = transition_target_entry or {}

    def anchor_of(entry_key: str, frame: tuple[int, int, bytes]) -> tuple[float, float]:
        w, h, pixels = frame
        return bbox_registration_point(w, h, pixels, scale_by_group[groups[entry_key]])

    def registration_point(entry_key: str, frame: tuple[int, int, bytes]) -> tuple[float, float]:
        """Punto de anclaje para REGISTRAR una transición contra su
        destino (usado únicamente dentro de la rama de `place()` de más
        abajo) -- el centroide ponderado por alpha ("centro de masa" de
        lo visible), NO el centro geométrico del bounding box que usa
        `anchor_of()`/`first_anchor` para la colocación DEFAULT de cada
        entrada.

        Por qué un punto de registro distinto (Block 05, pasada de
        pulido final -- ver docs/DECISION_LOG.md DEC-093, evidencia
        medida, no supuesta): al extender el mecanismo de anclaje por
        último-frame de DEC-087 a acciones self-loop (howl/tail_greet
        de Frin, groom/click de Bunny -- ver `align_endpoint_to_target_
        base`), registrar por CENTRO DE BBOX produjo resultados
        object­ivamente peores en el centroide de alpha real: medido en
        `groom` de Bunny, el centro de bbox del último frame quedó a
        <1px del de la base (excelente por ESA métrica), pero su
        centroide de masa REAL se disparó a ~4px de distancia (peor que
        antes de la corrección) -- el bbox de ese frame concreto tiene
        margen asimétrico (una pose con la postura del cuerpo
        desplazada dentro de un contorno de ancho similar), así que
        alinear sus DOS PIXELES EXTREMOS no alinea dónde está realmente
        el peso visual del personaje. `alpha_rms_radius()` (DEC-088) ya
        había establecido que la MEDIDA correcta de "tamaño" es
        ponderada por alpha, no por bbox -- esto aplica la misma lógica
        a la COLOCACIÓN: si el peso es lo que define el tamaño
        percibido, el centro de masa es lo que define la posición
        percibida.

        Cae a `anchor_of()` (bbox-center) solo si el frame es
        completamente transparente (`alpha_weighted_centroid()`
        devuelve None) -- un caso degenerado que nunca debería
        alcanzar contenido real, conservado por robustez, no porque se
        espere ejercitarlo."""
        w, h, pixels = frame
        return alpha_registration_point(w, h, pixels, scale_by_group[groups[entry_key]])

    first_anchor: dict[str, tuple[float, float]] = {}
    scaled_size: dict[str, tuple[float, float]] = {}
    for entry_key, (w, h, pixels) in entries.items():
        scale = scale_by_group[groups[entry_key]]
        first_anchor[entry_key] = anchor_of(entry_key, (w, h, pixels))
        scaled_size[entry_key] = (w * scale, h * scale)

    # --- Containment: una entrada cuyos frames son un SUBCONJUNTO de los
    # de otra (el contrato first/last-frame que este proyecto ya exige:
    # `state[lying].base_animation` ES, literalmente, el frame final de
    # `sit_to_lie`) NO es una medición independiente que haya que
    # reconciliar -- es el mismo archivo. Hereda la colocación de su
    # contenedor TAL CUAL, así que el frame compartido queda
    # pixel-por-pixel en el mismo lugar en las dos entradas, y la
    # transición de estado no puede saltar. Es la misma idea que el
    # union-find de escala de DEC-075, extendida de la ESCALA a la
    # TRASLACIÓN -- ver DEC-087.
    #
    # El match es por RUTA DE ARCHIVO real a nivel de ENTRADA (no de
    # grupo), así que right/left se emparejan con su propia dirección
    # sin depender del orden de los direction_overrides.
    container_of: dict[str, str] = {}
    if entry_paths:
        path_sets = {k: frozenset(v) for k, v in entry_paths.items() if v}
        for entry_key, own in path_sets.items():
            best: str | None = None
            for other_key, other in path_sets.items():
                if other_key == entry_key or not own < other:
                    continue
                # Determinista si hubiera más de un contenedor posible:
                # gana el de más frames, y a igualdad el de clave menor.
                if best is None or (len(path_sets[best]), best) < (len(other), other_key):
                    best = other_key
            if best is not None:
                container_of[entry_key] = best

    def resolve_container(entry_key: str, seen: set[str]) -> str:
        parent = container_of.get(entry_key)
        if parent is None or parent in seen:
            return entry_key
        seen.add(parent)
        return resolve_container(parent, seen)

    pos: dict[str, tuple[float, float]] = {}

    def place(entry_key: str) -> tuple[float, float]:
        if entry_key in pos:
            return pos[entry_key]
        root = resolve_container(entry_key, {entry_key})
        if root != entry_key:
            pos[entry_key] = place(root)
            return pos[entry_key]

        # Anclaje por PRIMER frame contra la base del estado de ORIGEN
        # (pasada de resolución de root-motion -- ver DEC-097). UNA
        # transforma rígida constante para todo el clip, igual que la
        # rama de anclaje-por-último-frame de más abajo: la única
        # diferencia es CUÁL de las dos puntas se registra exacto. Se
        # chequea primero porque, cuando el contenido lo pide, REEMPLAZA
        # al anclaje por destino (nunca se combinan: combinarlos es
        # justamente lo que exigiría interpolar, que es lo que QA
        # rechazó).
        source_entry = start_anchors.get(entry_key)
        if source_entry is not None and source_entry in entries:
            sx, sy = place(source_entry)
            source_w, source_h, source_px = entries[source_entry]
            sax, say = registration_point(source_entry, (source_w, source_h, source_px))
            fax, fay = registration_point(entry_key, entries[entry_key])
            pos[entry_key] = (sx + sax - fax, sy + say - fay)
            return pos[entry_key]

        target_entry = transition_targets.get(entry_key)
        last = last_frame_pixels.get(entry_key)
        if target_entry is not None and last is not None and target_entry in entries:
            # Transición que CAMBIA de estado y cuyo frame final NO es
            # el mismo archivo que la pose base del estado destino (el
            # `lie_to_sit` de Frin: un export inverso genuinamente
            # distinto). No hay containment que la ate, así que se ancla
            # por su ÚLTIMO frame contra donde la pose base del estado
            # destino realmente aterriza -- el instante en que el
            # personaje QUEDA QUIETO es donde un salto se ve; el
            # arranque de la transición ya está en movimiento y lo
            # disimula. Ver DEC-087 y docs/FRIN_CONTENT.md para la
            # medición de ambas puntas.
            tx, ty = place(target_entry)
            target_first_w, target_first_h, target_first_px = entries[target_entry]
            tax, tay = registration_point(target_entry, (target_first_w, target_first_h, target_first_px))
            lax, lay = registration_point(entry_key, last)
            pos[entry_key] = (tx + tax - lax, ty + tay - lay)
            return pos[entry_key]

        ax, ay = first_anchor[entry_key]
        pos[entry_key] = (-ax, -ay)
        return pos[entry_key]

    for entry_key in entries:
        place(entry_key)

    left = min(p[0] for p in pos.values())
    top = min(p[1] for p in pos.values())
    right = max(pos[k][0] + scaled_size[k][0] for k in entries)
    bottom = max(pos[k][1] + scaled_size[k][1] for k in entries)

    working_width = max(1, round(right - left))
    working_height = max(1, round(bottom - top))

    plan: dict[str, tuple[float, int, int, int, int]] = {}
    for entry_key in entries:
        px, py = pos[entry_key]
        plan[entry_key] = (
            scale_by_group[groups[entry_key]],
            working_width,
            working_height,
            round(px - left),
            round(py - top),
        )

    return plan


def write_master_from_canonical_frame(master_path: str, canonical_frame_path: str) -> None:
    """Escribe `master_path` como una copia re-codificada (decode +
    encode vía read_png_rgba/write_png_rgba de este mismo módulo, no
    un `cp` binario) de `canonical_frame_path` -- política determinista
    por-pet (Block 05, corrección post-QA: ver docs/DECISION_LOG.md).

    Por qué: antes de esto, `master.png` era un asset de referencia
    TOTALMENTE SEPARADO de los frames de animación reales -- inspeccionado
    en este bloque, resultó ser una ilustración "hero shot" de otro
    estilo/render (fondo sólido, a veces SIN canal alpha -- Nidir y
    ambos masters de Frin son colortype 2/RGB, ni siquiera decodificables
    por `read_png_rgba()`) que además nunca se compila al pack de
    runtime (`tools/compile_pet_pack.py` nunca lo lee -- confirmado). El
    owner sospechaba, correctamente, que master.png debería ser
    directamente el primer frame real de la pose base/canónica del pet:
    inspección visual confirmó que master.png y frame_000 SIEMPRE
    muestran el mismo personaje/pose (mismo diseño), solo que en un
    render/encuadre distinto -- reemplazar master.png por una copia
    real de frame_000 lo vuelve consistente, decodificable, y
    representativo de lo que el pet realmente muestra en pantalla, sin
    afectar en nada el pipeline de compilación (que sigue sin leer
    master.png para nada).

    Decodificar+recodificar (en vez de una copia de bytes directa)
    valida que el frame de origen sea un PNG RGBA8 real antes de
    escribir nada -- falla ruidosamente (ValueError de read_png_rgba)
    si no lo es, en vez de propagar un archivo potencialmente inválido."""
    width, height, pixels = read_png_rgba(canonical_frame_path)
    write_png_rgba(master_path, width, height, pixels)


def write_raw_rgba(path: str, width: int, height: int, pixels: bytes) -> None:
    with open(path, "wb") as f:
        f.write(b"NVR1")
        f.write(struct.pack("<II", width, height))
        f.write(pixels)


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)


def write_png_rgba(path: str, width: int, height: int, pixels: bytes) -> None:
    """Encodes raw RGBA8 bytes (row-major, top-to-bottom, straight
    alpha — the same layout read_png_rgba() produces) as a standard,
    minimal 8-bit RGBA PNG: filter type "None" on every scanline (no
    attempt at optimal compression — these are small, deterministic dev
    fixtures, not production art) and a single IDAT chunk. Any standard
    PNG reader, and this module's own read_png_rgba(), can read the
    result back byte-for-byte.
    """
    if len(pixels) != width * height * 4:
        raise ValueError(f"write_png_rgba: pixel buffer is {len(pixels)} bytes, expected {width * height * 4} for {width}x{height} RGBA8")

    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0 ("None") for every scanline
        raw += pixels[y * stride : (y + 1) * stride]
    compressed = zlib.compress(bytes(raw), 9)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_png_chunk(b"IHDR", ihdr))
        f.write(_png_chunk(b"IDAT", compressed))
        f.write(_png_chunk(b"IEND", b""))


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    width, height, pixels = read_png_rgba(src)
    write_raw_rgba(dst, width, height, pixels)
    print(f"{dst}: {width}x{height} RGBA8, {len(pixels)} pixel bytes ({4 + 8 + len(pixels)} total)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
