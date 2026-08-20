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


def compute_frame_normalization_plan(
    entries: dict[str, tuple[int, int, bytes]],
    groups: dict[str, str],
    reference_group: str,
    scale_tolerance: float = 0.02,
) -> dict[str, tuple[float, int, int, int, int]]:
    """Política genérica de "canvas de trabajo compartido, anclado por
    contenido" (Block 04.3 -- ver docs/NIDIR_CONTENT.md, "clipping y
    tamaño visual inconsistente entre animaciones"). Corrige el bug de
    raíz encontrado en QA manual de Nidir: cada frame se estiraba
    independientemente para llenar el mismo canvas lógico fijo
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
    "click_reaction", "passive_actions[0]", ...) -- una animación
    canónica y sus overrides direccionales SIEMPRE comparten grupo, así
    que right/left de una misma animación terminan con idéntico
    content_scale (nunca escalas distintas para las dos direcciones de
    la misma animación).
    `reference_group`: el grupo cuyo tamaño de contenido define la
    escala "1.0" contra la que se miden todos los demás -- "idle" por
    convención de este proyecto (ver DEC-045: idle ya es la referencia
    para el tamaño de canvas lógico; acá se reusa la misma convención
    para el tamaño de CONTENIDO).

    Para cada entrada calcula:
    - content_scale (compartido por grupo): factor de reescalado para
      que el contenido visible (bounding box de alpha) de este grupo
      ocupe el mismo tamaño absoluto en pixeles que el del grupo de
      referencia. Si la diferencia entra dentro de `scale_tolerance`
      (2% por defecto), se usa 1.0 -- evita un resample innecesario que
      degradaría calidad sin corregir nada perceptible ("Calidad
      visual consistente, sin degradación extra innecesaria").
    - Un canvas de trabajo COMPARTIDO por TODO el pet (mismas
      dimensiones para todas las entradas, de todos los grupos), lo
      bastante grande como para contener cada frame completo (post-
      content_scale) sin recortar nada, calculado alineando el CENTRO
      del bounding box de contenido del primer frame de cada entrada
      al centro del canvas de trabajo.
    - offset_x/offset_y: dónde colocar el frame (post-content_scale,
      sin recortar) dentro de ese canvas de trabajo compartido, para
      que su ancla de contenido caiga exactamente en el centro.

    Nunca recorta contenido -- solo agrega margen transparente
    (compose_on_canvas() nunca resamplea). Sin ninguna rama específica
    de personaje: cuánto escalar cada grupo y qué tan grande debe ser
    el canvas de trabajo se derivan enteramente de los pixeles reales,
    nunca de un valor hardcodeado por pet -- reusable tal cual para
    cualquier Nimvlet futuro con animaciones de distinto encuadre
    nativo."""
    if reference_group not in groups.values():
        raise ValueError(f"compute_frame_normalization_plan: reference_group '{reference_group}' is not used by any entry")

    def content_bbox_or_full_frame(w: int, h: int, pixels: bytes) -> tuple[int, int, int, int]:
        bbox = compute_content_bbox(w, h, pixels)
        if bbox is not None:
            return bbox
        # Frame totalmente transparente -- no hay contenido real que
        # anclar; el centro geométrico del frame es el único fallback
        # razonable (nunca se inventa contenido que no existe).
        return 0, 0, w - 1, h - 1

    canonical_of_group: dict[str, str] = {}
    for entry_key, group_key in groups.items():
        canonical_of_group.setdefault(group_key, entry_key)

    def group_content_size(group_key: str) -> float:
        entry_key = canonical_of_group[group_key]
        w, h, pixels = entries[entry_key]
        minx, miny, maxx, maxy = content_bbox_or_full_frame(w, h, pixels)
        return max(maxx - minx + 1, maxy - miny + 1)

    reference_size = group_content_size(reference_group)
    scale_by_group: dict[str, float] = {}
    for group_key in set(groups.values()):
        if group_key == reference_group:
            scale_by_group[group_key] = 1.0
            continue
        this_size = group_content_size(group_key)
        raw_scale = reference_size / this_size if this_size > 0 else 1.0
        scale_by_group[group_key] = 1.0 if abs(raw_scale - 1.0) <= scale_tolerance else raw_scale

    needed_left = needed_right = needed_top = needed_bottom = 0.0
    scaled_by_entry: dict[str, tuple[float, float, float, float]] = {}  # scaled_w, scaled_h, anchor_x, anchor_y
    for entry_key, (w, h, pixels) in entries.items():
        scale = scale_by_group[groups[entry_key]]
        minx, miny, maxx, maxy = content_bbox_or_full_frame(w, h, pixels)
        anchor_x = (minx + maxx + 1) / 2.0 * scale
        anchor_y = (miny + maxy + 1) / 2.0 * scale
        scaled_w = w * scale
        scaled_h = h * scale
        scaled_by_entry[entry_key] = (scaled_w, scaled_h, anchor_x, anchor_y)
        needed_left = max(needed_left, anchor_x)
        needed_right = max(needed_right, scaled_w - anchor_x)
        needed_top = max(needed_top, anchor_y)
        needed_bottom = max(needed_bottom, scaled_h - anchor_y)

    working_width = max(1, round(needed_left + needed_right))
    working_height = max(1, round(needed_top + needed_bottom))

    plan: dict[str, tuple[float, int, int, int, int]] = {}
    for entry_key, (_scaled_w, _scaled_h, anchor_x, anchor_y) in scaled_by_entry.items():
        offset_x = round(needed_left - anchor_x)
        offset_y = round(needed_top - anchor_y)
        plan[entry_key] = (scale_by_group[groups[entry_key]], working_width, working_height, offset_x, offset_y)

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
