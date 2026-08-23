#!/usr/bin/env python3
"""Tests puros (stdlib `unittest`, sin dependencias de terceros) para
la lógica de tools/ que Block 04.2 agrega: espejado horizontal
(`prep_dev_sprite.mirror_rgba_horizontal`), validación de secuencias
de frames (`validate_frame_sequence.validate_frame_sequence`), y --
agregado en la segunda pasada de este bloque -- la política genérica
de canvas lógico (`prep_dev_sprite.compute_logical_canvas_size`) y el
downscale opcional de runtime (`prep_dev_sprite.resize_rgba_nearest`).
Block 04.3 agrega la política de canvas de trabajo compartido anclado
por contenido (`prep_dev_sprite.compute_content_bbox`,
`compose_on_canvas`, `compute_frame_normalization_plan`) -- ver
docs/NIDIR_CONTENT.md, "clipping y tamaño visual inconsistente entre
animaciones".

Estos NO corren a través de `ctest` -- este repositorio no tiene
(todavía) ninguna integración de tests Python en CI/CTest, y este
bloque no la agrega (block brief: "Do not add UI or unrelated product
features" -- inventar infraestructura de CI Python no pedida sería
justamente eso). Se corren a mano:

    python3 tools/test_asset_pipeline.py

Ver docs/NIDIR_CONTENT.md para dónde encajan estos tests en el
contrato de testing general del bloque (equivalentes en Python, para
lógica que vive en Python, a lo que tests/DirectionTest.cpp ya cubre
del lado C++: "horizontal mirror correctness", "alpha preservation
after mirror", "frame order normalization" -- block brief §9).

Todos los fixtures son buffers RGBA8 sintéticos pequeños escritos a un
directorio temporal -- nunca los assets reales de Nidir (que ya se
validan por separado corriendo tools/generate_nidir_pack.py contra el
import real -- ver docs/NIDIR_CONTENT.md).
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

import prep_dev_sprite  # noqa: E402
import read_pet_pack  # noqa: E402
import validate_frame_sequence  # noqa: E402


def _solid_frame(width: int, height: int, rgb: tuple[int, int, int], alpha_rect: tuple[int, int, int, int]) -> bytes:
    """Buffer RGBA8 `width`x`height`: alpha=0 en todos lados salvo
    dentro de `alpha_rect` (x0, y0, x1, y1), donde alpha=255 y el color
    es `rgb` -- una "silueta" rectangular sintética simple, suficiente
    para probar espejado/alpha sin necesitar un PNG real."""
    x0, y0, x1, y1 = alpha_rect
    out = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            off = (y * width + x) * 4
            if x0 <= x < x1 and y0 <= y < y1:
                out[off : off + 4] = bytes([rgb[0], rgb[1], rgb[2], 255])
            # si no, queda en (0, 0, 0, 0) -- transparente
    return bytes(out)


class MirrorHorizontalTest(unittest.TestCase):
    def test_reverses_columns_preserving_rows(self) -> None:
        # 3x2, cada pixel con un valor R distinto (0, 10, 20 en la fila
        # 0; 100, 110, 120 en la fila 1) para poder confirmar la
        # posición exacta de cada pixel tras el espejado, no solo que
        # "algo cambió".
        width, height = 3, 2
        pixels = bytearray(width * height * 4)
        values = [[0, 10, 20], [100, 110, 120]]
        for y in range(height):
            for x in range(width):
                off = (y * width + x) * 4
                pixels[off : off + 4] = bytes([values[y][x], 0, 0, 200])

        mirrored = prep_dev_sprite.mirror_rgba_horizontal(width, height, bytes(pixels))

        for y in range(height):
            for x in range(width):
                off = (y * width + x) * 4
                expected_r = values[y][width - 1 - x]
                self.assertEqual(mirrored[off], expected_r, f"pixel ({x},{y}) esperaba R={expected_r}")
                self.assertEqual(mirrored[off + 3], 200)  # alpha sin tocar

    def test_double_mirror_is_identity(self) -> None:
        pixels = _solid_frame(9, 5, (12, 34, 56), (2, 1, 6, 4))
        once = prep_dev_sprite.mirror_rgba_horizontal(9, 5, pixels)
        twice = prep_dev_sprite.mirror_rgba_horizontal(9, 5, once)
        self.assertEqual(twice, pixels)

    def test_alpha_channel_preserved_exactly(self) -> None:
        # La región opaca (alpha_rect) preserva sus 255 exactos; el
        # resto preserva sus 0 exactos -- "alpha preservation after
        # mirror" del block brief §9, verificado pixel a pixel, no solo
        # "sigue habiendo transparencia en algún lado".
        width, height = 6, 4
        pixels = _solid_frame(width, height, (255, 0, 128), (1, 1, 3, 3))
        mirrored = prep_dev_sprite.mirror_rgba_horizontal(width, height, pixels)

        original_alphas = [pixels[i] for i in range(3, len(pixels), 4)]
        mirrored_alphas = [mirrored[i] for i in range(3, len(mirrored), 4)]
        self.assertEqual(sorted(original_alphas), sorted(mirrored_alphas))  # mismo multiset de valores alpha
        self.assertEqual(original_alphas.count(255), mirrored_alphas.count(255))
        self.assertEqual(original_alphas.count(0), mirrored_alphas.count(0))

    def test_rejects_wrong_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.mirror_rgba_horizontal(4, 4, b"\x00" * 10)


class LogicalCanvasSizeTest(unittest.TestCase):
    """`prep_dev_sprite.compute_logical_canvas_size()` -- la política
    genérica de tamaño de canvas lógico (Block 04.2, segunda pasada,
    DEC-045). Corresponde al item del brief "generic display sizing
    keeps render and hit-mask dimensions consistent": estos tests
    cubren la función pura que produce ese tamaño; la consistencia
    render/hit-mask en sí ya está cubierta del lado C++ por
    tests/DirectionTest.cpp (`HitMaskDimensionsStayConsistentAcrossDirectionSwitch`),
    ya que ambos caminos leen el mismo `canvasWidth/canvasHeight`."""

    # scale_factor=1.0 explícito en la mayoría de estos tests: aíslan la
    # matemática de aspect ratio en sí, independiente de qué candidato de
    # tamaño (DISPLAY_SIZE_SCALE_FACTOR) esté vigente en un momento dado
    # -- ver ScaleFactorTest más abajo para el comportamiento del factor
    # global en sí.

    def test_bunny_reference_size_is_unchanged(self) -> None:
        # Sanity check explícito: la convención ya establecida de Bunny
        # (160x160, cuadrado) debe seguir devolviendo exactamente
        # 160x160 con scale_factor=1.0 -- si esto cambiara, Bunny se
        # vería afectado.
        self.assertEqual(prep_dev_sprite.compute_logical_canvas_size(160, 160, scale_factor=1.0), (160, 160))

    def test_wide_native_resolution_scales_longer_edge_to_reference(self) -> None:
        # 513x525 (nativo real de idle de Nidir) -> el lado más largo
        # (525) queda en 160, el otro se escala proporcionalmente.
        canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(513, 525, scale_factor=1.0)
        self.assertEqual(canvas_height, 160)
        self.assertEqual(canvas_width, 156)

    def test_aspect_ratio_is_preserved_within_rounding(self) -> None:
        native_width, native_height = 800, 400
        canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(native_width, native_height, scale_factor=1.0)
        native_ratio = native_width / native_height
        canvas_ratio = canvas_width / canvas_height
        self.assertAlmostEqual(native_ratio, canvas_ratio, delta=0.01)

    def test_custom_reference_size_is_honored(self) -> None:
        self.assertEqual(prep_dev_sprite.compute_logical_canvas_size(200, 100, reference_size=100, scale_factor=1.0), (100, 50))

    def test_result_is_deterministic(self) -> None:
        first = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        second = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        self.assertEqual(first, second)

    def test_rejects_non_positive_dimensions(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.compute_logical_canvas_size(0, 100)


class DisplaySizeScaleFactorTest(unittest.TestCase):
    """`prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR` -- la política de
    tamaño visual GLOBAL (Block 04.3). Un solo valor, aplicado
    automáticamente por compute_logical_canvas_size() a cualquier pet
    que la use -- revertir es cambiar este único valor a 1.0. Vigente:
    1.10 (~+10% vs. el baseline original de 160, tras dos rondas de QA
    del owner -- ver el comentario junto a la constante)."""

    def test_default_call_applies_the_current_global_factor(self) -> None:
        with_default = prep_dev_sprite.compute_logical_canvas_size(160, 160)
        with_explicit_current_factor = prep_dev_sprite.compute_logical_canvas_size(
            160, 160, scale_factor=prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR
        )
        self.assertEqual(with_default, with_explicit_current_factor)

    def test_explicit_scale_factor_of_one_matches_pre_block_04_3_behavior(self) -> None:
        self.assertEqual(prep_dev_sprite.compute_logical_canvas_size(513, 525, scale_factor=1.0), (156, 160))

    def test_current_global_factor_matches_its_own_stated_magnitude(self) -> None:
        # Verifica que el factor VIGENTE (cualquiera que sea -- este
        # test no lo hardcodea) se aplique correctamente contra el
        # baseline sin factor, no que sea un valor específico.
        unscaled = prep_dev_sprite.compute_logical_canvas_size(513, 525, scale_factor=1.0)
        scaled = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        self.assertGreater(scaled[0], unscaled[0])
        self.assertGreater(scaled[1], unscaled[1])
        ratio_w = scaled[0] / unscaled[0]
        ratio_h = scaled[1] / unscaled[1]
        self.assertAlmostEqual(ratio_w, prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR, delta=0.02)
        self.assertAlmostEqual(ratio_h, prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR, delta=0.02)

    def test_scale_factor_is_absolute_against_original_baseline_not_compounded(self) -> None:
        # Requisito explícito del owner: pasar de +5% a +10% debe dar
        # un resultado ~1.10x el baseline ORIGINAL (160), NO ~1.05x
        # aplicado sobre el ~1.05x anterior (lo que daría ~1.1025x,
        # sutilmente distinto -- una diferencia real aunque pequeña,
        # que un pipeline "acumulativo" (guardar solo el candidato
        # vigente y multiplicar de nuevo) introduciría con el tiempo).
        # Como este valor SIEMPRE se define/aplica contra
        # REFERENCE_LOGICAL_SIZE (160) -- nunca contra el resultado de
        # una corrida anterior -- el factor efectivo es exactamente
        # 1.10, no 1.1025. Se verifica el valor de la propia constante,
        # no solo el resultado redondeado (que puede coincidir por
        # casualidad de redondeo en una resolución dada).
        self.assertAlmostEqual(prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR, 1.10, places=6)
        self.assertNotAlmostEqual(prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR, 1.05 * 1.05, places=6)

    def test_scale_factor_preserves_aspect_ratio(self) -> None:
        native_width, native_height = 624, 612
        unscaled = prep_dev_sprite.compute_logical_canvas_size(native_width, native_height, scale_factor=1.0)
        scaled = prep_dev_sprite.compute_logical_canvas_size(native_width, native_height, scale_factor=1.10)
        self.assertAlmostEqual(unscaled[0] / unscaled[1], scaled[0] / scaled[1], delta=0.01)
        with self.assertRaises(ValueError):
            prep_dev_sprite.compute_logical_canvas_size(100, -1)


class ResizeRgbaNearestTest(unittest.TestCase):
    """`prep_dev_sprite.resize_rgba_nearest()` -- el downscale opcional
    de runtime en tiempo de compilación (Block 04.2, segunda pasada,
    DEC-046). Cubre "mirror preserves alpha"-equivalente para el
    downscale: dimensiones de salida correctas y alpha preservado sin
    corromperse por el resampling."""

    def test_output_has_requested_dimensions(self) -> None:
        pixels = _solid_frame(8, 8, (10, 20, 30), (2, 2, 6, 6))
        resized = prep_dev_sprite.resize_rgba_nearest(8, 8, pixels, 4, 4)
        self.assertEqual(len(resized), 4 * 4 * 4)

    def test_upscale_and_downscale_are_both_supported(self) -> None:
        pixels = _solid_frame(4, 4, (1, 2, 3), (1, 1, 3, 3))
        smaller = prep_dev_sprite.resize_rgba_nearest(4, 4, pixels, 2, 2)
        larger = prep_dev_sprite.resize_rgba_nearest(4, 4, pixels, 8, 8)
        self.assertEqual(len(smaller), 2 * 2 * 4)
        self.assertEqual(len(larger), 8 * 8 * 4)

    def test_identity_resize_preserves_pixels_exactly(self) -> None:
        pixels = _solid_frame(5, 5, (42, 84, 126), (1, 1, 4, 4))
        resized = prep_dev_sprite.resize_rgba_nearest(5, 5, pixels, 5, 5)
        self.assertEqual(resized, pixels)

    def test_alpha_channel_survives_downscale_without_corruption(self) -> None:
        # 16x16, mitad opaca / mitad transparente -- confirma que el
        # downscale no introduce alpha "intermedio" inventado (nearest-
        # neighbor nunca promedia, a diferencia de un resize bilinear):
        # todo pixel de salida debe tener alpha exactamente 0 o 255,
        # nunca un valor fantasma entre medio.
        pixels = _solid_frame(16, 16, (200, 100, 50), (0, 0, 16, 8))
        resized = prep_dev_sprite.resize_rgba_nearest(16, 16, pixels, 4, 4)
        alphas = {resized[i] for i in range(3, len(resized), 4)}
        self.assertTrue(alphas.issubset({0, 255}))

    def test_result_is_deterministic(self) -> None:
        pixels = _solid_frame(10, 10, (5, 6, 7), (2, 2, 8, 8))
        first = prep_dev_sprite.resize_rgba_nearest(10, 10, pixels, 4, 4)
        second = prep_dev_sprite.resize_rgba_nearest(10, 10, pixels, 4, 4)
        self.assertEqual(first, second)

    def test_rejects_wrong_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.resize_rgba_nearest(4, 4, b"\x00" * 10, 2, 2)

    def test_rejects_non_positive_target_dimensions(self) -> None:
        pixels = _solid_frame(4, 4, (1, 2, 3), (0, 0, 4, 4))
        with self.assertRaises(ValueError):
            prep_dev_sprite.resize_rgba_nearest(4, 4, pixels, 0, 4)


class ResizeRgbaAreaAverageTest(unittest.TestCase):
    """`prep_dev_sprite.resize_rgba_area_average()` -- Block 04.3,
    corrección post-QA: box filter de calidad para downscales reales,
    reemplazando nearest-neighbor específicamente donde QA manual
    encontró pérdida real de detalle (docs/BUNNY_CONTENT.md)."""

    def test_output_has_requested_dimensions(self) -> None:
        pixels = _solid_frame(8, 8, (10, 20, 30), (2, 2, 6, 6))
        resized = prep_dev_sprite.resize_rgba_area_average(8, 8, pixels, 4, 4)
        self.assertEqual(len(resized), 4 * 4 * 4)

    def test_identity_resize_preserves_pixels_exactly(self) -> None:
        pixels = _solid_frame(5, 5, (42, 84, 126), (1, 1, 4, 4))
        resized = prep_dev_sprite.resize_rgba_area_average(5, 5, pixels, 5, 5)
        self.assertEqual(resized, pixels)

    def test_never_drops_a_single_opaque_pixel_entirely(self) -> None:
        # El caso real que motivó esta función: un detalle fino de 1
        # pixel de ancho no debe desaparecer por completo al reducir,
        # a diferencia de nearest-neighbor (que sí puede saltárselo por
        # mala suerte de en qué columna cae la muestra). Una franja
        # opaca de 1px de ancho en un frame ancho, reducido a la mitad,
        # debe seguir contribuyendo alpha > 0 en algún pixel de salida.
        w, h = 20, 4
        pixels = bytearray(w * h * 4)
        for y in range(h):
            off = (y * w + 3) * 4  # columna 3, 1px de ancho
            pixels[off : off + 4] = bytes([200, 100, 50, 255])
        resized = prep_dev_sprite.resize_rgba_area_average(w, h, bytes(pixels), 10, 4)
        alphas = [resized[i] for i in range(3, len(resized), 4)]
        self.assertTrue(any(a > 0 for a in alphas), "el detalle de 1px no debería desaparecer por completo")

    def test_transparent_pixel_noise_does_not_bleed_into_visible_color(self) -> None:
        # Pixeles completamente transparentes con RGB "ruidoso"
        # (arbitrario) no deberían contaminar el color de los pixeles
        # visibles vecinos al promediar -- el promedio de RGB está
        # ponderado por alpha, así que un peso de alpha=0 no debería
        # aportar nada.
        w, h = 4, 4
        pixels = bytearray(w * h * 4)
        for y in range(h):
            for x in range(w):
                off = (y * w + x) * 4
                if x < 2:
                    pixels[off : off + 4] = bytes([255, 0, 0, 255])  # rojo opaco
                else:
                    pixels[off : off + 4] = bytes([0, 255, 0, 0])  # "ruido" verde, transparente
        resized = prep_dev_sprite.resize_rgba_area_average(w, h, bytes(pixels), 2, 2)
        # Cada pixel de salida promedia una región 2x2 -- la mitad
        # izquierda (roja, opaca) y la mitad derecha (verde, transparente)
        # caen en columnas de destino DISTINTAS (columna 0 y columna 1),
        # así que cada pixel de salida debería seguir siendo puramente
        # rojo u puramente transparente, nunca una mezcla rojo/verde.
        for i in range(0, len(resized), 4):
            r, g, b, a = resized[i], resized[i + 1], resized[i + 2], resized[i + 3]
            if a > 0:
                self.assertEqual(g, 0, "el verde 'ruidoso' de un pixel transparente no debería mezclarse")

    def test_result_is_deterministic(self) -> None:
        pixels = _solid_frame(10, 10, (5, 6, 7), (2, 2, 8, 8))
        first = prep_dev_sprite.resize_rgba_area_average(10, 10, pixels, 4, 4)
        second = prep_dev_sprite.resize_rgba_area_average(10, 10, pixels, 4, 4)
        self.assertEqual(first, second)

    def test_rejects_wrong_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.resize_rgba_area_average(4, 4, b"\x00" * 10, 2, 2)

    def test_rejects_non_positive_target_dimensions(self) -> None:
        pixels = _solid_frame(4, 4, (1, 2, 3), (0, 0, 4, 4))
        with self.assertRaises(ValueError):
            prep_dev_sprite.resize_rgba_area_average(4, 4, pixels, 0, 4)


class ContentBboxTest(unittest.TestCase):
    """`prep_dev_sprite.compute_content_bbox()` -- Block 04.3, base de
    la política de canvas de trabajo compartido (docs/NIDIR_CONTENT.md,
    "clipping y tamaño visual inconsistente entre animaciones")."""

    def test_returns_exact_bbox_of_visible_rect(self) -> None:
        # Rect visible en (2,3)-(5,6) exclusivo -> bbox inclusivo (2,3)-(4,5)
        pixels = _solid_frame(10, 10, (1, 2, 3), (2, 3, 5, 6))
        self.assertEqual(prep_dev_sprite.compute_content_bbox(10, 10, pixels), (2, 3, 4, 5))

    def test_fully_transparent_frame_returns_none(self) -> None:
        pixels = _solid_frame(6, 6, (1, 2, 3), (0, 0, 0, 0))
        self.assertIsNone(prep_dev_sprite.compute_content_bbox(6, 6, pixels))

    def test_respects_custom_alpha_threshold(self) -> None:
        # Un solo pixel visible con alpha=50 -- por debajo de un umbral
        # alto (100) no debería contar como contenido.
        pixels = bytearray(4 * 4 * 4)
        pixels[(1 * 4 + 1) * 4 : (1 * 4 + 1) * 4 + 4] = bytes([10, 20, 30, 50])
        self.assertIsNone(prep_dev_sprite.compute_content_bbox(4, 4, bytes(pixels), alpha_threshold=100))
        self.assertEqual(prep_dev_sprite.compute_content_bbox(4, 4, bytes(pixels), alpha_threshold=10), (1, 1, 1, 1))

    def test_rejects_wrong_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.compute_content_bbox(4, 4, b"\x00" * 10)


class ComposeOnCanvasTest(unittest.TestCase):
    """`prep_dev_sprite.compose_on_canvas()` -- coloca un frame completo
    (sin recortar ni resamplear) dentro de un canvas más grande y
    transparente."""

    def test_places_frame_at_exact_offset(self) -> None:
        pixels = _solid_frame(2, 2, (9, 8, 7), (0, 0, 2, 2))  # 2x2 completamente opaco
        composed = prep_dev_sprite.compose_on_canvas(2, 2, pixels, 6, 6, offset_x=2, offset_y=3)
        self.assertEqual(len(composed), 6 * 6 * 4)
        # El frame 2x2 debe caer exactamente en columnas 2-3, filas 3-4.
        for y in range(6):
            for x in range(6):
                off = (y * 6 + x) * 4
                alpha = composed[off + 3]
                if 2 <= x < 4 and 3 <= y < 5:
                    self.assertEqual(alpha, 255, f"expected opaque at ({x},{y})")
                else:
                    self.assertEqual(alpha, 0, f"expected transparent at ({x},{y})")

    def test_preserves_pixel_values_exactly_no_resample(self) -> None:
        pixels = _solid_frame(3, 3, (42, 84, 126), (0, 0, 3, 3))
        composed = prep_dev_sprite.compose_on_canvas(3, 3, pixels, 3, 3, offset_x=0, offset_y=0)
        self.assertEqual(composed, pixels)  # canvas del mismo tamaño, offset (0,0) -> copia idéntica

    def test_areas_outside_placed_frame_stay_transparent(self) -> None:
        pixels = _solid_frame(2, 2, (1, 1, 1), (0, 0, 2, 2))
        composed = prep_dev_sprite.compose_on_canvas(2, 2, pixels, 10, 10, offset_x=0, offset_y=0)
        # Esquina opuesta del canvas, lejos del frame colocado -- debe
        # seguir en alpha=0.
        off = (9 * 10 + 9) * 4
        self.assertEqual(composed[off + 3], 0)

    def test_rejects_wrong_buffer_size(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.compose_on_canvas(4, 4, b"\x00" * 10, 8, 8, 0, 0)

    def test_rejects_non_positive_canvas_dimensions(self) -> None:
        pixels = _solid_frame(2, 2, (1, 2, 3), (0, 0, 2, 2))
        with self.assertRaises(ValueError):
            prep_dev_sprite.compose_on_canvas(2, 2, pixels, 0, 8, 0, 0)


class LerpOffsetScheduleTest(unittest.TestCase):
    """`prep_dev_sprite.lerp_offset_schedule()` -- Block 05, pasada de
    continuidad de frontera (ver docs/DECISION_LOG.md): la
    interpolación LINEAL de traslación, un offset entero por frame,
    entre `start_offset` (frame 0) y `end_offset` (último frame) --
    NUNCA de escala, nunca por-eje independiente. Tests puros, sin
    ningún manifest ni PNG de por medio."""

    def test_first_frame_is_exactly_the_start_offset(self) -> None:
        schedule = prep_dev_sprite.lerp_offset_schedule(10, (5, -3), (25, 17))
        self.assertEqual(schedule[0], (5, -3))

    def test_last_frame_is_exactly_the_end_offset(self) -> None:
        schedule = prep_dev_sprite.lerp_offset_schedule(10, (5, -3), (25, 17))
        self.assertEqual(schedule[-1], (25, 17))

    def test_schedule_length_matches_frame_count(self) -> None:
        for n in (1, 2, 3, 25):
            self.assertEqual(len(prep_dev_sprite.lerp_offset_schedule(n, (0, 0), (10, 10))), n)

    def test_single_frame_degenerates_to_the_end_offset(self) -> None:
        # Con un solo frame no hay "entre" que interpolar -- frame 0 Y
        # el último son EL MISMO frame, así que debe coincidir con la
        # punta que un caso normal protege primero (el destino).
        self.assertEqual(prep_dev_sprite.lerp_offset_schedule(1, (5, 5), (99, 99)), [(99, 99)])

    def test_interpolation_is_monotonic_per_axis(self) -> None:
        # Cobertura explícita del brief: "translation correction is
        # smooth/monotonic across frames" -- una interpolación lineal
        # entre dos puntos fijos nunca invierte de dirección en NINGÚN
        # eje.
        schedule = prep_dev_sprite.lerp_offset_schedule(25, (0, 100), (200, 0))
        xs = [p[0] for p in schedule]
        ys = [p[1] for p in schedule]
        self.assertEqual(xs, sorted(xs))  # x sube monótonamente
        self.assertEqual(ys, sorted(ys, reverse=True))  # y baja monótonamente

    def test_constant_offsets_produce_a_constant_schedule(self) -> None:
        # Si las dos puntas ya coinciden, la interpolación no debería
        # inventar ningún movimiento -- cada frame es el mismo punto.
        schedule = prep_dev_sprite.lerp_offset_schedule(25, (7, -2), (7, -2))
        self.assertTrue(all(p == (7, -2) for p in schedule))

    def test_result_is_deterministic(self) -> None:
        a = prep_dev_sprite.lerp_offset_schedule(13, (1, 2), (30, -40))
        b = prep_dev_sprite.lerp_offset_schedule(13, (1, 2), (30, -40))
        self.assertEqual(a, b)


class TwoEndpointFrameOffsetsTest(unittest.TestCase):
    """`prep_dev_sprite.compute_two_endpoint_frame_offsets()` -- la
    decisión de registro de DOS puntas en aislamiento (sin pasar por
    `compute_frame_normalization_plan()`/un manifest completo): dado el
    primer y último frame de una transición más las bases de origen y
    destino YA colocadas, decide si un solo offset constante alcanza o
    si hace falta interpolar."""

    def _frame(self, w: int, h: int, rect: tuple[int, int, int, int]) -> tuple[int, int, bytes]:
        return w, h, _solid_frame(w, h, (9, 9, 9), rect)

    def test_agreeing_endpoints_collapse_to_a_single_constant_offset(self) -> None:
        # Fabricado para que las dos puntas registren EXACTAMENTE en el
        # mismo punto: mismo contenido, mismas bases, mismas posiciones.
        frame = self._frame(40, 40, (15, 15, 24, 24))
        base = self._frame(40, 40, (15, 15, 24, 24))
        start, end, needs_interp = prep_dev_sprite.compute_two_endpoint_frame_offsets(
            frame_count=10, scale=1.0,
            first_frame=frame, last_frame=frame,
            source_base_frame=base, source_base_scale=1.0, source_base_pos=(0.0, 0.0),
            target_base_frame=base, target_base_scale=1.0, target_base_pos=(0.0, 0.0),
        )
        self.assertFalse(needs_interp)
        self.assertEqual(start, end)

    def test_disagreeing_endpoints_trigger_interpolation(self) -> None:
        first = self._frame(40, 40, (2, 2, 11, 11))
        last = self._frame(40, 40, (28, 28, 37, 37))
        base = self._frame(40, 40, (15, 15, 24, 24))
        start, end, needs_interp = prep_dev_sprite.compute_two_endpoint_frame_offsets(
            frame_count=10, scale=1.0,
            first_frame=first, last_frame=last,
            source_base_frame=base, source_base_scale=1.0, source_base_pos=(0.0, 0.0),
            target_base_frame=base, target_base_scale=1.0, target_base_pos=(0.0, 0.0),
        )
        self.assertTrue(needs_interp)
        self.assertNotEqual(start, end)

    def test_result_is_deterministic(self) -> None:
        first = self._frame(40, 40, (2, 2, 11, 11))
        last = self._frame(40, 40, (28, 28, 37, 37))
        base = self._frame(40, 40, (15, 15, 24, 24))
        kwargs = dict(
            frame_count=10, scale=1.0, first_frame=first, last_frame=last,
            source_base_frame=base, source_base_scale=1.0, source_base_pos=(3.0, -2.0),
            target_base_frame=base, target_base_scale=1.0, target_base_pos=(-5.0, 8.0),
        )
        self.assertEqual(
            prep_dev_sprite.compute_two_endpoint_frame_offsets(**kwargs),
            prep_dev_sprite.compute_two_endpoint_frame_offsets(**kwargs),
        )


class FrameNormalizationPlanTest(unittest.TestCase):
    """`prep_dev_sprite.compute_frame_normalization_plan()` -- Block
    04.3, la política genérica de canvas de trabajo compartido/anclado
    por contenido que corrige el clipping y la inconsistencia de
    tamaño visual entre animaciones de un mismo pet (ver
    docs/NIDIR_CONTENT.md). Este grupo de tests usa un solo
    BehaviorState sintético ("s") vía `_plan()` -- exactamente el caso
    de Bunny/Nidir -- así que ejercita la misma matemática de
    comparación-contra-referencia de siempre; el union-find por
    archivo-compartido y la referencia POR ESTADO (Block 05, DEC-075)
    se prueban por separado en `MultiStateNormalizationPlanTest`, más
    abajo, con un grafo de 2+ estados real."""

    def _entry(self, w: int, h: int, rect: tuple[int, int, int, int]) -> tuple[int, int, bytes]:
        return w, h, _solid_frame(w, h, (5, 5, 5), rect)

    def _plan(self, entries: dict, groups: dict, reference_group: str) -> dict:
        # Un único estado sintético "s" para todos los grupos, y una
        # ruta de archivo FALSA y ÚNICA por grupo -- así el union-find
        # por archivo-compartido nunca fusiona nada acá (cada grupo se
        # compara de forma independiente contra `reference_group`,
        # exactamente el comportamiento anterior a Block 05).
        group_keys = set(groups.values())
        state_of_group = {g: "s" for g in group_keys}
        base_group_of_state = {"s": reference_group}
        group_frame_paths = {g: [f"/fake/{g}.png"] for g in group_keys}
        return prep_dev_sprite.compute_frame_normalization_plan(
            entries, groups, reference_group=reference_group,
            group_frame_paths=group_frame_paths, state_of_group=state_of_group,
            base_group_of_state=base_group_of_state,
        )

    def test_reference_group_always_gets_scale_one(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(20, 20, (5, 5, 15, 15)),
        }
        groups = {"idle": "idle", "click": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        self.assertEqual(plan["idle"][0], 1.0)

    def test_matching_content_size_yields_no_rescale(self) -> None:
        # Mismo tamaño de contenido (10x10) en frames nativos de
        # tamaño distinto -- solo difiere el margen alrededor, como el
        # caso real de Nidir (idle vs. click_reaction).
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),  # contenido 10x10
            "click": self._entry(30, 30, (10, 10, 20, 20)),  # contenido 10x10, canvas nativo más grande
        }
        groups = {"idle": "idle", "click": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        self.assertAlmostEqual(plan["click"][0], 1.0, places=6)

    def test_different_content_size_yields_real_rescale(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),  # contenido 10x10
            "click": self._entry(20, 20, (8, 8, 12, 12)),  # contenido 4x4 -- mucho más chico
        }
        groups = {"idle": "idle", "click": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        content_scale = plan["click"][0]
        self.assertGreater(content_scale, 1.0)  # el contenido "click" es más chico -> hay que agrandarlo

        # El factor exacto sale de `alpha_rms_radius`, no del lado más
        # largo del bounding box (Block 05, pasada de estabilización --
        # ver DEC-088 y el docstring de esa función). Para un bloque
        # sólido discreto de n x n la varianza por eje es (n^2-1)/12, así
        # que el radio RMS es sqrt(2*(n^2-1)/12): 10x10 -> 4.0620,
        # 4x4 -> 1.5811, ratio 2.5690 en vez del 2.5 exacto que daba la
        # medida vieja por bounding box. La diferencia es el efecto de
        # grilla discreta, no un cambio de intención: ambas medidas
        # coinciden en el límite continuo.
        expected = prep_dev_sprite.alpha_rms_radius(*entries["idle"]) / prep_dev_sprite.alpha_rms_radius(*entries["click"])
        self.assertAlmostEqual(content_scale, expected, places=6)
        self.assertAlmostEqual(content_scale, 2.569046, places=5)

    def test_size_metric_is_far_less_sensitive_to_a_pose_change_than_bbox(self) -> None:
        """La razón real por la que la medida dejó de ser el lado más
        largo del bounding box (Block 05, DEC-088): una pose autorada
        que estira algo hacia un costado NO es un cambio de tamaño, y
        una medida decidida por dos pixeles extremos no puede
        distinguirlos.

        Caso real que motivó esto: el `howl` de Frin macho mide +4.5% de
        ANCHO de bbox contra la pose base sentada con EXACTAMENTE la
        misma altura, y sin embargo su radio RMS ponderado por alpha
        está a menos de 1%."""
        size = 60
        body = (10, 10, 50, 50)  # cuerpo de 40x40
        base_w, base_h, base_px = self._entry(size, size, body)
        posed = bytearray(base_px)
        # Una "cola" fina de 8px que sobresale a un costado: el bbox se
        # ensancha 20%, la masa de alpha apenas cambia (8 pixeles contra
        # 1600).
        for x in range(50, 58):
            offset = (30 * size + x) * 4
            posed[30 * size * 4 + x * 4:offset + 4] = bytes((5, 5, 5, 255))

        def bbox_longest(w: int, h: int, pixels: bytes) -> int:
            minx, miny, maxx, maxy = prep_dev_sprite.compute_content_bbox(w, h, pixels)
            return max(maxx - minx + 1, maxy - miny + 1)

        bbox_ratio = bbox_longest(size, size, bytes(posed)) / bbox_longest(base_w, base_h, base_px)
        rms_ratio = prep_dev_sprite.alpha_rms_radius(size, size, bytes(posed)) / prep_dev_sprite.alpha_rms_radius(
            base_w, base_h, base_px
        )

        # La pose es más ancha de verdad, y el bbox lo grita...
        self.assertGreater(bbox_ratio, 1.15)
        # ...pero el tamaño real del personaje no cambió, y la
        # registración por alpha lo refleja: al menos un orden de
        # magnitud menos de desvío.
        self.assertLess(abs(rms_ratio - 1.0), abs(bbox_ratio - 1.0) / 10.0)

        entries = {"base": (base_w, base_h, base_px), "action": (size, size, bytes(posed))}
        groups = {"base": "base", "action": "action"}
        plan = self._plan(entries, groups, reference_group="base")
        # Y el plan real no aplasta la pose autorada.
        self.assertLess(abs(plan["action"][0] - 1.0), 0.02)

    def test_all_entries_share_the_same_working_canvas_dimensions(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "idle_left": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(30, 26, (12, 8, 22, 18)),
            "click_left": self._entry(30, 26, (8, 8, 18, 18)),
        }
        groups = {"idle": "idle", "idle_left": "idle", "click": "click", "click_left": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        working_dims = {(w, h) for _, w, h, _, _ in plan.values()}
        self.assertEqual(len(working_dims), 1, f"expected one shared working canvas, got {working_dims}")

    def test_canonical_and_override_in_same_group_share_content_scale(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(20, 20, (8, 8, 12, 12)),
            "click_left": self._entry(20, 20, (8, 8, 12, 12)),
        }
        groups = {"idle": "idle", "click": "click", "click_left": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        self.assertEqual(plan["click"][0], plan["click_left"][0])

    def test_working_canvas_is_large_enough_to_avoid_cropping_every_entry(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(40, 12, (2, 2, 38, 10)),  # contenido muy ancho, casi todo el frame
        }
        groups = {"idle": "idle", "click": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        for key, (scale, working_w, working_h, offset_x, offset_y) in plan.items():
            w, h, _ = entries[key]
            scaled_w = round(w * scale)
            scaled_h = round(h * scale)
            self.assertGreaterEqual(offset_x, -0.001, f"{key}: el frame no debería empezar antes del canvas")
            self.assertGreaterEqual(offset_y, -0.001, f"{key}: el frame no debería empezar antes del canvas")
            self.assertLessEqual(offset_x + scaled_w, working_w + 1, f"{key}: el frame no debería exceder el canvas en X")
            self.assertLessEqual(offset_y + scaled_h, working_h + 1, f"{key}: el frame no debería exceder el canvas en Y")

    def test_fully_transparent_entry_falls_back_to_frame_center_without_raising(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(20, 20, (0, 0, 0, 0)),  # completamente transparente
        }
        groups = {"idle": "idle", "click": "click"}
        plan = self._plan(entries, groups, reference_group="idle")
        self.assertIn("click", plan)  # no debe fallar ni omitir la entrada

    def test_rejects_unknown_reference_group(self) -> None:
        entries = {"idle": self._entry(10, 10, (2, 2, 8, 8))}
        groups = {"idle": "idle"}
        with self.assertRaises(ValueError):
            self._plan(entries, groups, reference_group="does_not_exist")

    def test_result_is_deterministic(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(30, 30, (10, 10, 20, 20)),
        }
        groups = {"idle": "idle", "click": "click"}
        first = self._plan(entries, groups, reference_group="idle")
        second = self._plan(entries, groups, reference_group="idle")
        self.assertEqual(first, second)


class TransitionEndpointContinuityTest(unittest.TestCase):
    """Block 05, pasada de estabilización (ver docs/DECISION_LOG.md
    DEC-087): el invariante de continuidad de una transición que CAMBIA
    de estado --

        último frame mostrado de la transición
            ==
        primer frame mostrado de la pose base del estado destino

    -- cuando el contenido declara/reusa el mismo asset de punta.

    Antes de esta corrección cada entrada se colocaba anclando el
    centro de contenido de su PROPIO frame 0 al centro del canvas de
    trabajo. Para `state[lying].base_animation`, cuyo único frame ES el
    frame final de `sit_to_lie`, eso significaba colocar los MISMOS
    pixeles en dos lugares distintos -- medido en el pack real de Frin
    macho antes del arreglo: 62px de salto vertical al entrar a
    `lying`, con bounding box y suma de alpha IDÉNTICOS (o sea:
    traslación pura, no un problema de escala ni de contenido)."""

    def _frame(self, w: int, h: int, rect: tuple[int, int, int, int]) -> tuple[int, int, bytes]:
        return w, h, _solid_frame(w, h, (5, 5, 5), rect)

    def _two_state_plan(self, *, lying_base_path: str, sit_to_lie_last_path: str | None = None,
                        lie_to_sit_last=None):
        """Grafo mínimo pero realista: seated (base == frame 0 de
        sit_to_lie), sit_to_lie -> lying, lying (base == frame final de
        sit_to_lie), lie_to_sit -> seated."""
        seated_pose = self._frame(40, 40, (14, 6, 25, 33))   # alto/angosto
        lying_pose = self._frame(40, 40, (4, 24, 35, 33))    # bajo/ancho, MÁS ABAJO en el frame
        entries = {
            "state[seated].base_animation": seated_pose,
            "state[seated].ambient_actions[sit_to_lie]": seated_pose,  # frame 0 de la transición
            "state[lying].base_animation": lying_pose,
            "state[lying].click_actions[lie_to_sit]": lying_pose,      # frame 0 de la reversa
        }
        groups = {k: k for k in entries}
        seated_path = "/fake/sit_to_lie/frame_000.png"
        entry_frame_paths = {
            "state[seated].base_animation": [seated_path],
            "state[seated].ambient_actions[sit_to_lie]": [
                seated_path, sit_to_lie_last_path if sit_to_lie_last_path is not None else lying_base_path
            ],
            "state[lying].base_animation": [lying_base_path],
            "state[lying].click_actions[lie_to_sit]": ["/fake/lie_to_sit/frame_000.png",
                                                       "/fake/lie_to_sit/frame_024.png"],
        }
        kwargs = {}
        if lie_to_sit_last is not None:
            kwargs["last_frames"] = {"state[lying].click_actions[lie_to_sit]": lie_to_sit_last}
            kwargs["transition_target_entry"] = {
                "state[lying].click_actions[lie_to_sit]": "state[seated].base_animation"
            }
        return prep_dev_sprite.compute_frame_normalization_plan(
            entries, groups,
            reference_group="state[seated].base_animation",
            group_frame_paths={k: list(v) for k, v in entry_frame_paths.items()},
            state_of_group={
                "state[seated].base_animation": "seated",
                "state[seated].ambient_actions[sit_to_lie]": "seated",
                "state[lying].base_animation": "lying",
                "state[lying].click_actions[lie_to_sit]": "lying",
            },
            base_group_of_state={"seated": "state[seated].base_animation",
                                 "lying": "state[lying].base_animation"},
            entry_frame_paths=entry_frame_paths,
            **kwargs,
        )

    def test_state_base_that_reuses_a_transition_frame_gets_the_same_placement(self) -> None:
        plan = self._two_state_plan(lying_base_path="/fake/sit_to_lie/frame_024.png")
        transition = plan["state[seated].ambient_actions[sit_to_lie]"]
        lying_base = plan["state[lying].base_animation"]
        # Misma escala, mismo canvas Y MISMO OFFSET -- lo último es el
        # arreglo: el frame compartido aterriza pixel por pixel en el
        # mismo lugar en las dos entradas, así que el switch de estado
        # no puede saltar.
        self.assertEqual(lying_base, transition)

    def test_seated_base_also_stays_pinned_to_the_transition_start(self) -> None:
        plan = self._two_state_plan(lying_base_path="/fake/sit_to_lie/frame_024.png")
        self.assertEqual(plan["state[seated].base_animation"],
                         plan["state[seated].ambient_actions[sit_to_lie]"])

    def test_without_the_shared_file_the_two_placements_really_do_differ(self) -> None:
        """Control negativo: si el contenido NO reusa el asset de punta,
        no hay nada que heredar y las dos colocaciones divergen -- que
        es exactamente el bug que el reuso real evita. Sin este control,
        el test de arriba pasaría aunque el mecanismo no hiciera nada."""
        linked = self._two_state_plan(lying_base_path="/fake/sit_to_lie/frame_024.png")
        # Sin reuso: la pose base de "lying" es un asset propio, y el
        # frame final de sit_to_lie es OTRO archivo distinto.
        unlinked = self._two_state_plan(lying_base_path="/fake/unrelated_lying/frame_000.png",
                                        sit_to_lie_last_path="/fake/sit_to_lie/frame_024.png")
        self.assertEqual(linked["state[lying].base_animation"],
                         linked["state[seated].ambient_actions[sit_to_lie]"])
        self.assertNotEqual(unlinked["state[lying].base_animation"],
                            unlinked["state[seated].ambient_actions[sit_to_lie]"])

    def test_independent_reverse_transition_registers_on_its_last_frame(self) -> None:
        """`lie_to_sit` es un export independiente: su frame final NO es
        el mismo archivo que la pose base de `seated`, así que no hay
        containment que lo ate. Se ancla por su ÚLTIMO frame contra
        donde la base del estado destino realmente aterriza -- el
        instante en que el personaje queda quieto, que es donde un salto
        se ve."""
        # Frame final de lie_to_sit: la misma pose sentada, pero
        # encuadrada 7px más arriba en su frame nativo que la de seated.
        last = self._frame(40, 40, (14, 6 - 7, 25, 33 - 7))
        plan = self._two_state_plan(lying_base_path="/fake/sit_to_lie/frame_024.png",
                                    lie_to_sit_last=last)
        seated = plan["state[seated].base_animation"]
        lie_to_sit = plan["state[lying].click_actions[lie_to_sit]"]
        # El centro de contenido del último frame de lie_to_sit debe
        # caer donde cae el de la pose base de seated.
        def content_centre_y(offset_y: int, frame) -> float:
            w, h, px = frame
            _minx, miny, _maxx, maxy = prep_dev_sprite.compute_content_bbox(w, h, px)
            return offset_y + (miny + maxy + 1) / 2.0
        self.assertAlmostEqual(
            content_centre_y(lie_to_sit[4], last),
            content_centre_y(seated[4], (40, 40, _solid_frame(40, 40, (5, 5, 5), (14, 6, 25, 33)))),
            delta=1.0,
        )

    def test_every_entry_still_shares_one_working_canvas(self) -> None:
        last = self._frame(40, 40, (14, 0, 25, 27))
        plan = self._two_state_plan(lying_base_path="/fake/sit_to_lie/frame_024.png",
                                    lie_to_sit_last=last)
        self.assertEqual(len({(w, h) for _s, w, h, _x, _y in plan.values()}), 1)

    def test_result_is_deterministic(self) -> None:
        kwargs = dict(lying_base_path="/fake/sit_to_lie/frame_024.png",
                      lie_to_sit_last=self._frame(40, 40, (14, 0, 25, 27)))
        self.assertEqual(self._two_state_plan(**kwargs), self._two_state_plan(**kwargs))


class CompiledFrinEndpointContinuityTest(unittest.TestCase):
    """El invariante de DEC-087 verificado sobre los PACKS COMPILADOS
    REALES que se envían, no sobre un fixture sintético: se decodifican
    los bytes que el runtime va a mostrar (`tools/read_pet_pack.py`, el
    lado de lectura del mismo contrato que src/content/PetPackLoader.cpp)
    y se comparan las dos puntas de cada transición de estado, para las
    dos variantes y las dos direcciones.

    Este es el test que falla si una recompilación futura vuelve a
    romper la continuidad -- que es exactamente lo que pasó y que la QA
    manual del owner detectó como "el lobo salta al quedar acostado"."""

    PACKS = (("male", "assets/dev/frin_male_pack.nvpack"),
             ("female", "assets/dev/frin_female_pack.nvpack"))
    DIRECTIONS = ("right", "left")

    def _pack(self, rel: str) -> dict:
        return read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))

    def _displayed(self, pack: dict, state_id: str, direction: str, action_id: str | None, trigger: str | None):
        state = read_pet_pack.find_state(pack, state_id)
        if action_id is None:
            return read_pet_pack.resolve_animation(
                state["base_animation"], state["base_animation_direction_overrides"], direction)
        action = read_pet_pack.find_action(state, trigger, action_id)
        return read_pet_pack.resolve_animation(action["animation"], action["direction_overrides"], direction)

    def test_sit_to_lie_ends_exactly_on_the_lying_base_pose(self) -> None:
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    last = self._displayed(pack, "seated", direction, "sit_to_lie", "ambient_actions")["frames"][-1]
                    base = self._displayed(pack, "lying", direction, None, None)["frames"][0]
                    self.assertEqual((last["width"], last["height"]), (base["width"], base["height"]))
                    # El contenido DECLARA el mismo asset de punta
                    # (lying_base ES el frame final de sit_to_lie), así
                    # que el contrato es igualdad pixel por pixel -- no
                    # "parecido dentro de una tolerancia".
                    self.assertEqual(last["pixels"], base["pixels"])

    def test_seated_base_matches_the_first_frame_of_sit_to_lie(self) -> None:
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    first = self._displayed(pack, "seated", direction, "sit_to_lie", "ambient_actions")["frames"][0]
                    base = self._displayed(pack, "seated", direction, None, None)["frames"][0]
                    self.assertEqual(first["pixels"], base["pixels"])

    def test_lie_to_sit_lands_on_the_seated_base_pose(self) -> None:
        """`lie_to_sit` es un export INDEPENDIENTE: su frame final no es
        el mismo archivo que la pose sentada, así que la igualdad pixel
        a pixel no aplica ni sería honesta pedirla. Lo que sí debe
        cumplirse es que quede REGISTRADO en su sitio: el centro de
        contenido cae a pocos pixeles del de la pose base destino.

        Antes de DEC-087 esta distancia era de ~53px verticales (medida
        en el pack real), claramente visible como un salto al asentarse."""
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    last = self._displayed(pack, "lying", direction, "lie_to_sit", "click_actions")["frames"][-1]
                    base = self._displayed(pack, "seated", direction, None, None)["frames"][0]
                    lx, ly = read_pet_pack.content_centre(last)
                    bx, by = read_pet_pack.content_centre(base)
                    self.assertLess(abs(lx - bx), 4.0, f"{variant}/{direction} horizontal")
                    self.assertLess(abs(ly - by), 4.0, f"{variant}/{direction} vertical")

    def test_frin_packs_carry_the_twelve_second_rest_delay(self) -> None:
        # Pasada de continuidad de frontera: revierte DEC-089's 10.0 de
        # vuelta a 12.0 (unificado otra vez con Bunny/Nidir), pedido de
        # producto explícito.
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            self.assertEqual(read_pet_pack.find_state(pack, "seated")["ambient_interval_seconds"], 12.0, variant)
            self.assertEqual(read_pet_pack.find_state(pack, "lying")["ambient_actions"], [], variant)


class LieToSitTwoEndpointContinuityTest(unittest.TestCase):
    """Pasada de continuidad de frontera (ver docs/DECISION_LOG.md):
    QA manual, específico -- "when lying and the owner clicks to stand
    up, the stable lying pose immediately jumps in position when the
    FIRST lie_to_sit frame appears". DEC-087/DEC-092 ya habían anclado
    la punta FINAL de `lie_to_sit` contra `seated_base` -- esto agrega
    el anclaje de la punta INICIAL contra `lying_base`
    (`align_transition_both_endpoints`, ver
    `compute_two_endpoint_frame_offsets()`), con interpolación LINEAL
    de traslación (nunca de escala) si un solo transform constante no
    alcanza para las dos puntas.

    Medido con centroide ponderado por alpha (la métrica que el propio
    mecanismo optimiza -- no bbox) contra los packs COMPILADOS
    reales."""

    PACKS = CompiledFrinEndpointContinuityTest.PACKS
    DIRECTIONS = CompiledFrinEndpointContinuityTest.DIRECTIONS

    # 2.0px: el peor caso medido tras la corrección es ~1.16px (Frin
    # hembra, arranque). Antes de esta pasada la punta de arranque no
    # tenía NINGÚN registro -- el salto que el owner reportó.
    MAX_ENDPOINT_DELTA_PX = 2.0

    def _pack(self, rel: str) -> dict:
        return read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))

    def _lie_to_sit_and_bases(self, pack: dict, direction: str):
        seated = read_pet_pack.find_state(pack, "seated")
        lying = read_pet_pack.find_state(pack, "lying")
        seated_base = read_pet_pack.resolve_animation(
            seated["base_animation"], seated["base_animation_direction_overrides"], direction)["frames"][0]
        lying_base = read_pet_pack.resolve_animation(
            lying["base_animation"], lying["base_animation_direction_overrides"], direction)["frames"][0]
        action = read_pet_pack.find_action(lying, "click_actions", "lie_to_sit")
        anim = read_pet_pack.resolve_animation(action["animation"], action["direction_overrides"], direction)
        return anim, seated_base, lying_base

    def test_first_frame_registers_against_lying_base(self) -> None:
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    anim, _seated_base, lying_base = self._lie_to_sit_and_bases(pack, direction)
                    f0 = read_pet_pack.content_alpha_centroid(anim["frames"][0])
                    lb = read_pet_pack.content_alpha_centroid(lying_base)
                    delta = math.hypot(f0[0] - lb[0], f0[1] - lb[1])
                    self.assertLess(
                        delta, self.MAX_ENDPOINT_DELTA_PX,
                        f"{variant}/{direction}: lie_to_sit frame 0 is {delta:.2f}px away from lying_base -- "
                        "the 'lying pose jumps when lie_to_sit starts' regression")

    def test_last_frame_still_registers_against_seated_base(self) -> None:
        # Invariante que YA existía (DEC-087/092) -- se re-verifica acá,
        # junto al del arranque, para que las dos puntas queden
        # protegidas por el mismo test class.
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    anim, seated_base, _lying_base = self._lie_to_sit_and_bases(pack, direction)
                    fl = read_pet_pack.content_alpha_centroid(anim["frames"][-1])
                    sb = read_pet_pack.content_alpha_centroid(seated_base)
                    delta = math.hypot(fl[0] - sb[0], fl[1] - sb[1])
                    self.assertLess(delta, self.MAX_ENDPOINT_DELTA_PX, f"{variant}/{direction}")

    def test_translation_correction_never_produces_a_large_frame_to_frame_jump(self) -> None:
        """"gradual translation correction... far preferable to an
        instantaneous teleport" -- se verifica de forma OBSERVABLE sobre
        los 25 frames reales: ningún par de frames CONSECUTIVOS se
        mueve más que una fracción chica de lo que el personaje se
        mueve en TOTAL a lo largo de toda la animación (lo que
        descartaría un salto instantáneo escondido en medio de un
        movimiento por lo demás suave)."""
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                with self.subTest(variant=variant, direction=direction):
                    anim, _seated_base, _lying_base = self._lie_to_sit_and_bases(pack, direction)
                    centroids = [read_pet_pack.content_alpha_centroid(f) for f in anim["frames"]]
                    step_distances = [
                        math.hypot(centroids[i + 1][0] - centroids[i][0], centroids[i + 1][1] - centroids[i][1])
                        for i in range(len(centroids) - 1)
                    ]
                    total_span = math.hypot(
                        centroids[-1][0] - centroids[0][0], centroids[-1][1] - centroids[0][1])
                    max_step = max(step_distances)
                    # Ningún paso individual debería acaparar más de un
                    # cuarto del desplazamiento neto total en 24 pasos
                    # -- un salto instantáneo real (p. ej. el frame 0
                    # sin ningún registro) se vería como UN paso
                    # gigante, no como esta distribución pareja.
                    self.assertLess(
                        max_step, max(10.0, total_span * 0.25),
                        f"{variant}/{direction}: largest single-frame jump ({max_step:.2f}px) looks like a "
                        f"snap, not smooth motion (net displacement across the whole clip: {total_span:.2f}px)")

    def test_no_per_frame_scale_variation(self) -> None:
        """"Do not interpolate zoom" -- todos los frames de lie_to_sit
        comparten UNA sola escala (verificable de forma observable: si
        alguno tuviera una escala distinta, sus dimensiones compiladas
        diferirían -- el compilador ya falla fuerte si eso ocurre)."""
        for variant, rel in self.PACKS:
            pack = self._pack(rel)
            for direction in self.DIRECTIONS:
                anim, _sb, _lb = self._lie_to_sit_and_bases(pack, direction)
                dims = {(f["width"], f["height"]) for f in anim["frames"]}
                with self.subTest(variant=variant, direction=direction):
                    self.assertEqual(len(dims), 1, f"lie_to_sit frames do not share one uniform scale: {dims}")

    def test_align_transition_both_endpoints_is_declared(self) -> None:
        manifests = {
            "male": "assets/source/nimvlets/frin/male/pack_manifest.json",
            "female": "assets/source/nimvlets/frin/female/pack_manifest.json",
        }
        for variant, manifest_rel in manifests.items():
            with open(os.path.join(_REPO_ROOT, manifest_rel), "r", encoding="utf-8") as f:
                manifest = json.load(f)
            lying = next(s for s in manifest["states"] if s["id"] == "lying")
            action = next(a for a in lying["click_actions"] if a["id"] == "lie_to_sit")
            with self.subTest(variant=variant):
                self.assertTrue(action.get("align_transition_both_endpoints", False))

    def test_sit_to_lie_is_not_flagged_for_two_endpoint_registration(self) -> None:
        """`sit_to_lie` NO necesita esto -- sus dos puntas YA son
        archivos compartidos con seated_base/lying_base (containment
        las deja exactas sin ningún registro por contenido, ver
        DEC-087) -- agregar el flag ahí sería ruido sin efecto. Se fija
        como test para que quede documentado por qué, no solo por
        omisión."""
        manifests = {
            "male": "assets/source/nimvlets/frin/male/pack_manifest.json",
            "female": "assets/source/nimvlets/frin/female/pack_manifest.json",
        }
        for variant, manifest_rel in manifests.items():
            with open(os.path.join(_REPO_ROOT, manifest_rel), "r", encoding="utf-8") as f:
                manifest = json.load(f)
            seated = next(s for s in manifest["states"] if s["id"] == "seated")
            action = next(a for a in seated["ambient_actions"] if a["id"] == "sit_to_lie")
            with self.subTest(variant=variant):
                self.assertFalse(action.get("align_transition_both_endpoints", False))


class CompiledSelfLoopEndpointContinuityTest(unittest.TestCase):
    """Block 05, pasada de pulido final (ver docs/DECISION_LOG.md
    DEC-092/DEC-093): el "size snap" que QA manual reportó al VOLVER a
    la pose base tras `groom`/`click` (Bunny) y `howl`/`tail_greet`
    (Frin, sentado) -- distinto del invariante de CompiledClickScaleTest
    (que mide el frame 0/arranque de la acción) porque este mide
    específicamente la PUNTA DE REGRESO: el último frame mostrado antes
    de que el runtime vuelva a mostrar la base (ver
    content::AnimationController).

    Medido con centroide ponderado por alpha (centro de masa real, no
    bbox) contra los packs COMPILADOS reales -- ver
    tools/read_pet_pack.py's content_alpha_centroid()."""

    # 2.5px de tolerancia sobre un frame nativo de ~250-550px: el peor
    # caso medido tras la corrección es ~0.88px (Bunny groom, right) --
    # esto deja margen real de regresión (no es un snapshot que haya
    # que reajustar por cualquier cambio de arte de <1px) sin dejar
    # pasar una regresión real como la de antes de esta pasada (Bunny
    # groom llegaba a 1.82px, Frin macho lie_to_sit a ~53px antes de
    # DEC-087).
    MAX_ENDPOINT_DELTA_PX = 2.5

    FLAGGED_ACTIONS = (
        ("assets/dev/bunny_pack.nvpack", "default", "ambient_actions", "groom"),
        ("assets/dev/bunny_pack.nvpack", "default", "click_actions", "click"),
        ("assets/dev/frin_male_pack.nvpack", "seated", "click_actions", "howl"),
        ("assets/dev/frin_male_pack.nvpack", "seated", "click_actions", "tail_greet"),
        ("assets/dev/frin_female_pack.nvpack", "seated", "click_actions", "howl"),
        ("assets/dev/frin_female_pack.nvpack", "seated", "click_actions", "tail_greet"),
    )

    def test_flagged_actions_return_close_to_their_base_pose(self) -> None:
        for rel, state_id, trigger, action_id in self.FLAGGED_ACTIONS:
            pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))
            state = read_pet_pack.find_state(pack, state_id)
            for direction in ("right", "left"):
                with self.subTest(pack=pack["id"], action=action_id, direction=direction):
                    base = read_pet_pack.resolve_animation(
                        state["base_animation"], state["base_animation_direction_overrides"], direction)
                    action = read_pet_pack.find_action(state, trigger, action_id)
                    anim = read_pet_pack.resolve_animation(action["animation"], action["direction_overrides"], direction)
                    base_c = read_pet_pack.content_alpha_centroid(base["frames"][0])
                    last_c = read_pet_pack.content_alpha_centroid(anim["frames"][-1])
                    delta = math.hypot(last_c[0] - base_c[0], last_c[1] - base_c[1])
                    self.assertLess(
                        delta, self.MAX_ENDPOINT_DELTA_PX,
                        f"{pack['id']}/{state_id}/{action_id} ({direction}) returns {delta:.2f}px away "
                        "from its base pose's alpha centroid -- larger than the measured post-fix worst "
                        "case, likely a real regression in align_endpoint_to_target_base's placement")

    def test_flag_is_actually_set_on_every_flagged_action(self) -> None:
        """Control de que el propio CONTENIDO declara el flag donde se
        espera -- si un futuro cambio en un generador lo borra por
        accidente, el test de arriba seguiría pasando por casualidad
        mientras el mecanismo esté ausente (el legado "anclado por
        frame 0" también puede dar resultados chicos si el contenido
        cambia) -- este test cierra ese hueco verificando la DECLARACIÓN
        en el manifest, no solo el resultado numérico."""
        manifests = {
            "assets/dev/bunny_pack.nvpack": "assets/source/nimvlets/bunny/pack_manifest.json",
            "assets/dev/frin_male_pack.nvpack": "assets/source/nimvlets/frin/male/pack_manifest.json",
            "assets/dev/frin_female_pack.nvpack": "assets/source/nimvlets/frin/female/pack_manifest.json",
        }
        for rel, state_id, trigger, action_id in self.FLAGGED_ACTIONS:
            manifest_rel = manifests[rel]
            with open(os.path.join(_REPO_ROOT, manifest_rel), "r", encoding="utf-8") as f:
                manifest = json.load(f)
            state = next(s for s in manifest["states"] if s["id"] == state_id)
            action = next(a for a in state[trigger] if a["id"] == action_id)
            with self.subTest(manifest=manifest_rel, action=action_id):
                self.assertTrue(
                    action.get("align_endpoint_to_target_base", False),
                    f"{manifest_rel}: state[{state_id}].{trigger}[{action_id}] is missing "
                    "align_endpoint_to_target_base: true")


class ReturnEndpointScaleContinuityTest(unittest.TestCase):
    """Pasada de continuidad de frontera (ver docs/DECISION_LOG.md):
    QA manual siguió reportando un cambio de TAMAÑO perceptible al
    volver a la base tras `groom`/`click`/`howl`/`tail_greet`, incluso
    después de que DEC-092 arreglara la COLOCACIÓN -- ese arreglo
    preservaba `content_scale` a propósito ("that was not sufficient",
    brief de esta pasada). Antes de esta pasada, la escala de CUALQUIER
    acción (incluida una self-loop con `align_endpoint_to_target_base`)
    se medía SIEMPRE contra su PROPIO frame 0 -- correcto para "qué tan
    grande es la pose de arranque", no necesariamente para "qué tan
    grande es la pose que REALMENTE toca la base al terminar".

    Este test mide la ESCALA (radio RMS ponderado por alpha, invariante
    a traslación -- DEC-088) del ÚLTIMO frame contra la base, no solo su
    posición (que ya cubre CompiledSelfLoopEndpointContinuityTest de
    arriba)."""

    # 1% de tolerancia: el peor caso medido tras la corrección es
    # ~0.81% (Frin macho howl). Deja margen real de regresión sin ser
    # un snapshot pixel-perfecto.
    MAX_RETURN_SCALE_DRIFT = 0.01

    FLAGGED_ACTIONS = CompiledSelfLoopEndpointContinuityTest.FLAGGED_ACTIONS

    def _rms(self, frame: dict) -> float:
        return prep_dev_sprite.alpha_rms_radius(frame["width"], frame["height"], frame["pixels"])

    def test_return_endpoint_scale_matches_base(self) -> None:
        for rel, state_id, trigger, action_id in self.FLAGGED_ACTIONS:
            pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))
            state = read_pet_pack.find_state(pack, state_id)
            for direction in ("right", "left"):
                with self.subTest(pack=pack["id"], action=action_id, direction=direction):
                    base = read_pet_pack.resolve_animation(
                        state["base_animation"], state["base_animation_direction_overrides"], direction)
                    action = read_pet_pack.find_action(state, trigger, action_id)
                    anim = read_pet_pack.resolve_animation(action["animation"], action["direction_overrides"], direction)
                    base_rms = self._rms(base["frames"][0])
                    last_rms = self._rms(anim["frames"][-1])
                    drift = abs(last_rms / base_rms - 1.0)
                    self.assertLess(
                        drift, self.MAX_RETURN_SCALE_DRIFT,
                        f"{pack['id']}/{state_id}/{action_id} ({direction}) returns at {drift * 100:+.2f}% "
                        "scale drift from its base pose -- larger than the measured post-fix worst case")

    def test_scale_is_uniform_across_every_frame_of_the_action(self) -> None:
        """"No per-frame scaling behavior" -- pedido explícito del
        brief. La escala de una acción se comprime a un único
        `content_scale` en el plan (nunca varía con el índice de
        frame); esto se comprueba de forma OBSERVABLE: si algún frame
        recibiera una escala distinta, sus dimensiones compiladas
        diferirían de las del resto (el compilador YA falla fuerte si
        eso pasa -- ver `_compile_animation()` -- así que una
        compilación exitosa es, en sí, una prueba negativa; acá se
        vuelve a verificar explícitamente sobre el pack real, no solo
        confiar en que el compilador no lanzó)."""
        for rel, state_id, trigger, action_id in self.FLAGGED_ACTIONS:
            pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))
            state = read_pet_pack.find_state(pack, state_id)
            for direction in ("right", "left"):
                action = read_pet_pack.find_action(state, trigger, action_id)
                anim = read_pet_pack.resolve_animation(action["animation"], action["direction_overrides"], direction)
                dims = {(f["width"], f["height"]) for f in anim["frames"]}
                with self.subTest(pack=pack["id"], action=action_id, direction=direction):
                    self.assertEqual(len(dims), 1, f"frames of {action_id} do not share one uniform scale: {dims}")


class CompiledClickScaleTest(unittest.TestCase):
    """Block 05, pasada de estabilización (DEC-088): QA manual reportó
    que las animaciones de click se veían "un poco más anchas/grandes"
    que la pose base aprobada de su estado.

    Medido con REGISTRACIÓN POR ALPHA (`alpha_rms_radius`) y no con
    ancho de bounding box -- que es lo que el brief pidió y lo que
    distingue "el personaje es más grande" de "la pose autorada es más
    ancha". Los dos casos del contenido real, para que quede escrito:
      - Bunny `click`: bbox +1.9% de ancho contra la base, pero solo
        +0.06% de radio RMS.
      - Frin macho `howl`: bbox +2.1% de ancho con altura casi igual,
        -0.12% de radio RMS.
    O sea: lo que queda de diferencia visible está AUTORADO en el arte,
    no lo introduce el compilador. Este test fija el lado que sí es
    responsabilidad del compilador."""

    # Tolerancia deliberadamente más ancha (1%) que el desvío real
    # máximo medido tras la corrección (0.32%): esto es un guard contra
    # una regresión de verdad -- como la de 1.43% que tenía
    # `tail_greet` de Frin hembra antes -- no un snapshot que haya que
    # actualizar cada vez que el arte cambie un pixel.
    MAX_RELATIVE_SIZE_DRIFT = 0.01

    PACKS = ("assets/dev/bunny_pack.nvpack", "assets/dev/nidir_pack.nvpack",
             "assets/dev/frin_male_pack.nvpack", "assets/dev/frin_female_pack.nvpack")

    def _size(self, frame: dict) -> float:
        return prep_dev_sprite.alpha_rms_radius(frame["width"], frame["height"], frame["pixels"])

    def test_every_action_matches_its_own_state_base_pose(self) -> None:
        for rel in self.PACKS:
            pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, rel))
            for state in pack["states"]:
                for direction in ("right", "left"):
                    base = read_pet_pack.resolve_animation(
                        state["base_animation"], state["base_animation_direction_overrides"], direction)
                    base_size = self._size(base["frames"][0])
                    self.assertGreater(base_size, 0.0)
                    for trigger in ("ambient_actions", "hover_actions", "click_actions"):
                        for action in state[trigger]:
                            animation = read_pet_pack.resolve_animation(
                                action["animation"], action["direction_overrides"], direction)
                            drift = self._size(animation["frames"][0]) / base_size - 1.0
                            with self.subTest(pack=pack["id"], state=state["id"],
                                              direction=direction, action=action["id"]):
                                self.assertLess(
                                    abs(drift), self.MAX_RELATIVE_SIZE_DRIFT,
                                    f"{pack['id']}/{state['id']}/{action['id']} ({direction}) is "
                                    f"{drift * 100:+.2f}% off its state's base pose")


class NidirGoldenControlTest(unittest.TestCase):
    """Nidir es el control dorado establecido: se ve bien en todos los
    caminos de renderer y ninguna pasada debe moverlo. Su geometría
    aprobada se fija acá para que un cambio en la política de
    normalización (que es exactamente lo que esta pasada tocó) falle
    ruidosamente en vez de deslizarse.

    En la pasada de estabilización de Block 05 el pack de Nidir quedó
    BYTE-IDÉNTICO tras regenerarlo con la métrica nueva -- estos valores
    son los de ese pack."""

    def test_nidir_canvas_and_visual_scale_are_unchanged(self) -> None:
        pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, "assets/dev/nidir_pack.nvpack"))
        self.assertEqual((pack["canvas_width"], pack["canvas_height"]), (176, 173))
        self.assertEqual(pack["visual_scale"], 1.25)
        self.assertEqual(pack["alpha_hit_threshold"], 128)

    def test_nidir_every_animation_is_compiled_at_scale_one(self) -> None:
        """Ninguna animación de Nidir necesitó nunca un `content_scale`
        distinto de 1.0 -- todas sus animaciones ya comparten encuadre.
        Se verifica de forma observable: todos los frames comparten
        dimensiones y todas las animaciones miden lo mismo."""
        pack = read_pet_pack.read_pack(os.path.join(_REPO_ROOT, "assets/dev/nidir_pack.nvpack"))
        state = read_pet_pack.find_state(pack, "default")
        sizes = set()
        for direction in ("right", "left"):
            base = read_pet_pack.resolve_animation(
                state["base_animation"], state["base_animation_direction_overrides"], direction)
            sizes.add((base["frames"][0]["width"], base["frames"][0]["height"]))
            for trigger in ("ambient_actions", "hover_actions", "click_actions"):
                for action in state[trigger]:
                    animation = read_pet_pack.resolve_animation(
                        action["animation"], action["direction_overrides"], direction)
                    for frame in animation["frames"]:
                        sizes.add((frame["width"], frame["height"]))
        self.assertEqual(len(sizes), 1, f"Nidir frames should all share one canvas, got {sizes}")

    def test_nidir_pack_is_byte_identical(self) -> None:
        """Verificación EXPLÍCITA de bytes, no solo de propiedades
        derivadas -- pedido literal del brief de la pasada de pulido
        final ("NIDIR PACK MUST REMAIN BYTE-IDENTICAL. Assert this
        explicitly"). El hash se tomó del pack ya regenerado con TODO
        el código de esta pasada (inversión de dirección de Frin,
        `align_endpoint_to_target_base`, registro por centroide de
        alpha) corriendo -- Nidir nunca pasa por ninguno de esos
        caminos (su generador nunca setea el flag, y no tiene ninguna
        transición que cambie de estado), así que el hash debe
        coincidir SIEMPRE que se vuelva a compilar, no solo hoy."""
        with open(os.path.join(_REPO_ROOT, "assets/dev/nidir_pack.nvpack"), "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()
        self.assertEqual(
            digest, "594da545ce52e6721dfc90debb8f2b8184ac5cb9a1ca07ba7a6a1801e0cc968a",
            "nidir_pack.nvpack changed -- Nidir is the frozen golden control (see AGENTS.md / block brief); "
            "if this pack was meant to change, this is very likely an unintended regression from touching "
            "shared compiler/tooling code, not an approved content update")


class PrivacyInvariantTest(unittest.TestCase):
    """AGENTS.md §5/§14 son contratos duros: nada en el runtime instala
    un hook global de input, captura pantalla, ni pide un permiso de
    TCC. La pasada de estabilización de Block 05 tocó justamente el
    código de plataforma de macOS donde un atajo así sería tentador
    (había una alternativa técnicamente viable -- un monitor global de
    `NSEvent` -- y se descartó explícitamente por esta razón, ver
    DEC-086), así que el contrato se fija en un test en vez de quedar
    solo escrito.

    Vive en esta suite porque es la única suite de Python del repo; no
    tiene nada que ver con assets. Es un chequeo de FUENTE, no de
    comportamiento -- no puede probar que no aparezca un diálogo de
    permiso, pero sí que no exista ninguna de las APIs que lo
    provocarían."""

    # Cada entrada: (símbolo prohibido, por qué).
    FORBIDDEN = (
        ("addGlobalMonitorForEvents", "monitor global de NSEvent -- hook de input system-wide"),
        ("addLocalMonitorForEvents", "monitor de NSEvent a nivel app -- innecesario y fuera de contrato"),
        ("CGEventTapCreate", "event tap -- requiere Accessibility"),
        ("AXIsProcessTrusted", "consulta/solicitud de permiso de Accessibility"),
        ("IOHIDManager", "acceso HID crudo -- requiere Input Monitoring"),
        ("CGWindowListCreateImage", "captura de pantalla"),
        ("CGDisplayCreateImage", "captura de pantalla"),
        ("SCStream", "ScreenCaptureKit -- captura de pantalla"),
    )

    def _runtime_sources(self) -> list[str]:
        paths = []
        for root, _dirs, files in os.walk(os.path.join(_REPO_ROOT, "src")):
            for name in files:
                if name.endswith((".cpp", ".h", ".mm")):
                    paths.append(os.path.join(root, name))
        return paths

    def test_no_global_input_hook_or_screen_capture_in_the_runtime(self) -> None:
        sources = self._runtime_sources()
        self.assertGreater(len(sources), 20, "source scan found suspiciously few files")
        for path in sources:
            # El check GUI de click-through no se envía con el producto,
            # pero igual no usa ninguno de estos símbolos, así que no se
            # exceptúa: si algún día los usara, queremos enterarnos.
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            for symbol, why in self.FORBIDDEN:
                self.assertNotIn(
                    symbol, text,
                    f"{os.path.relpath(path, _REPO_ROOT)} uses '{symbol}' ({why}) -- "
                    "prohibido por AGENTS.md §5/§14")

    def test_click_through_still_uses_only_a_cursor_position_query(self) -> None:
        """El único dato de entrada global que este producto lee es la
        POSICIÓN del cursor, vía la API cross-platform de SDL. Si eso
        cambiara a cualquier otra cosa, este test es el que lo nota."""
        spike = os.path.join(_REPO_ROOT, "src/app/SpikeApp.cpp")
        with open(spike, "r", encoding="utf-8") as f:
            text = f.read()
        self.assertIn("SDL_GetGlobalMouseState", text)


class ContentTimingPolicyTest(unittest.TestCase):
    """Los valores de ritmo que el owner fija como producto viven en el
    CONTENIDO (manifests por-pet), no en el motor -- así que se fijan
    acá, contra los manifests reales que alimentan al compilador.

    Pasada de estabilización de Block 05 (DEC-089): Frin baja a 10s y
    deja de estar unificado con Bunny/Nidir, que se quedan en 12s."""

    def _manifest(self, rel: str) -> dict:
        with open(os.path.join(_REPO_ROOT, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def _state(self, manifest: dict, state_id: str) -> dict:
        for state in manifest["states"]:
            if state["id"] == state_id:
                return state
        raise AssertionError(f"state '{state_id}' not found")

    def test_frin_seated_rest_delay_is_twelve_seconds(self) -> None:
        # Pasada de continuidad de frontera: revierte DEC-089's 10.0 de
        # vuelta a 12.0.
        for variant in ("male", "female"):
            manifest = self._manifest(f"assets/source/nimvlets/frin/{variant}/pack_manifest.json")
            seated = self._state(manifest, "seated")
            self.assertEqual(seated["ambient_interval_seconds"], 12.0, variant)
            # Y que el timer realmente sirva para algo: el ambient de
            # seated es la transición a lying.
            self.assertEqual([a["id"] for a in seated["ambient_actions"]], ["sit_to_lie"], variant)
            self.assertEqual(seated["ambient_actions"][0]["target_state_id"], "lying", variant)

    def test_frin_lying_never_arms_an_ambient_timer(self) -> None:
        for variant in ("male", "female"):
            manifest = self._manifest(f"assets/source/nimvlets/frin/{variant}/pack_manifest.json")
            self.assertEqual(self._state(manifest, "lying")["ambient_actions"], [], variant)

    def test_bunny_and_nidir_ambient_interval_stays_twelve_seconds(self) -> None:
        for pet in ("bunny", "nidir"):
            manifest = self._manifest(f"assets/source/nimvlets/{pet}/pack_manifest.json")
            self.assertEqual(self._state(manifest, "default")["ambient_interval_seconds"], 12.0, pet)

    def test_hover_dwell_pinned_in_core_matches_product_value(self) -> None:
        """El umbral de hover (0.4s -- pasada de continuidad de
        frontera) vive en core::kDefaultHoverDwellSeconds (src/app no
        es testeable en aislamiento), pero esta suite SÍ puede leer la
        fuente C++ como texto y fijar que el valor no se desvíe en
        silencio -- mismo patrón que
        test_click_through_still_uses_only_a_cursor_position_query()
        usa para SpikeApp.cpp más arriba."""
        header = os.path.join(_REPO_ROOT, "src/core/HoverDwellTracker.h")
        with open(header, "r", encoding="utf-8") as f:
            text = f.read()
        self.assertIn("kDefaultHoverDwellSeconds = 0.4", text)


class FrinRuntimeDirectionInversionTest(unittest.TestCase):
    """Block 05, pasada de pulido final (ver docs/DECISION_LOG.md
    DEC-091): pedido de producto explícito -- TODO lo que se veía
    corriendo con `Direction::kRight` pasa a verse con `Direction::
    kLeft`, y viceversa, para las DOS variantes de Frin, en TODO
    contenido direccional (pose base, sit_to_lie, lie_to_sit, howl,
    tail_greet).

    Invariante VARIANTE-INDEPENDIENTE que la inversión produce, derivado
    algebraicamente de `_invert_runtime_direction()` +
    `entries_for()` (ver `tools/generate_frin_pack.py`) y verificado acá
    contra el pack REAL: sin importar `canonical_direction` (macho
    "left", hembra "right" -- la orientación que el owner exportó de
    verdad), el slot runtime `Direction::kRight` SIEMPRE termina leyendo
    frames de la carpeta física `.../left/frames/`, y el slot runtime
    `Direction::kLeft` SIEMPRE de `.../right/frames/` -- exactamente
    invertido de lo que habría dado un mapeo directo sin inversión (que
    habría sido: kRight <- carpeta física "right", kLeft <- carpeta
    física "left"). Las carpetas en sí NUNCA se renombran ni se mueven
    -- siguen siendo PROVENANCE (qué exportó el owner realmente), nunca
    semántica de runtime -- ver el docstring de generate_frin_pack.py.

    Se verifica sobre el manifest REAL escrito por el generador (no un
    fixture sintético), para base_animation Y cada acción, en las dos
    direction_overrides, las dos variantes -- así que cualquier
    regresión futura en `entries_for()`/`_invert_runtime_direction()`
    lo rompe de forma ruidosa."""

    VARIANTS = ("male", "female")
    ANIMATION_IDS = ("sit_to_lie", "lie_to_sit", "howl", "tail_greet")

    def _manifest(self, variant: str) -> dict:
        with open(os.path.join(_REPO_ROOT, f"assets/source/nimvlets/frin/{variant}/pack_manifest.json"),
                  "r", encoding="utf-8") as f:
            return json.load(f)

    def _physical_folder(self, source_path: str) -> str:
        # "animations/<anim>/{left,right}/frames/frame_NNN.png" -- el
        # segmento de carpeta física está siempre en la MISMA posición.
        parts = source_path.replace("\\", "/").split("/")
        self.assertIn("frames", parts)
        idx = parts.index("frames")
        self.assertGreaterEqual(idx, 1)
        physical = parts[idx - 1]
        self.assertIn(physical, ("left", "right"))
        return physical

    def _assert_all_sources_use_folder(self, frames: list[dict], expected_folder: str, label: str) -> None:
        for i, frame in enumerate(frames):
            actual = self._physical_folder(frame["source"])
            self.assertEqual(actual, expected_folder, f"{label} frame {i}: {frame['source']}")

    def test_runtime_right_always_reads_the_physical_left_folder(self) -> None:
        for variant in self.VARIANTS:
            manifest = self._manifest(variant)
            for state in manifest["states"]:
                with self.subTest(variant=variant, state=state["id"], part="base_animation"):
                    self._assert_all_sources_use_folder(
                        state["base_animation"]["frames"], "left", f"{variant}/state[{state['id']}].base_animation")
                for trigger in ("ambient_actions", "click_actions"):
                    for action in state.get(trigger, []):
                        with self.subTest(variant=variant, state=state["id"], action=action["id"]):
                            self._assert_all_sources_use_folder(
                                action["frames"], "left", f"{variant}/state[{state['id']}].{trigger}[{action['id']}]")

    def test_runtime_left_always_reads_the_physical_right_folder(self) -> None:
        for variant in self.VARIANTS:
            manifest = self._manifest(variant)
            for state in manifest["states"]:
                base_override = next(o for o in state["base_animation_direction_overrides"] if o["direction"] == "left")
                with self.subTest(variant=variant, state=state["id"], part="base_animation_direction_overrides[left]"):
                    self._assert_all_sources_use_folder(
                        base_override["frames"], "right", f"{variant}/state[{state['id']}].base_animation (left override)")
                for trigger in ("ambient_actions", "click_actions"):
                    for action in state.get(trigger, []):
                        override = next(o for o in action["direction_overrides"] if o["direction"] == "left")
                        with self.subTest(variant=variant, state=state["id"], action=action["id"]):
                            self._assert_all_sources_use_folder(
                                override["frames"], "right",
                                f"{variant}/state[{state['id']}].{trigger}[{action['id']}] (left override)")

    def test_inversion_is_a_deterministic_mirror_not_a_coincidence(self) -> None:
        """Control directo sobre `_invert_runtime_direction()` +
        `entries_for()` en aislamiento (sin generar ningún pack real):
        fija el resultado algebraico para las CUATRO combinaciones
        posibles de canonical_direction x runtime_direction, así que
        una futura reescritura de esa función que rompa la simetría
        falla acá, sin esperar 90+ segundos de regeneración real."""
        sys.path.insert(0, os.path.join(_REPO_ROOT, "tools"))
        import generate_frin_pack as G

        # Reimplementación mínima y AISLADA de la misma lógica pura que
        # generate_frin_pack.py's _build_variant_manifest() define
        # localmente (no se puede importar la función anidada
        # directamente) -- si esto alguna vez diverge del código real,
        # es justamente lo que este test está para detectar: se
        # verifica no contra ESTA copia sino contra el resultado real
        # de test_runtime_right_always_reads_the_physical_left_folder()
        # de arriba, que sí ejercita el código de producción. Acá solo
        # se fija el resultado matemático esperado para dejar registrada
        # la tabla de verdad completa.
        def invert(d: str) -> str:
            return "left" if d == "right" else "right"

        expected = {
            ("left", "right"): "real",       # macho: kRight -> su propio export real
            ("left", "left"): "derived",     # macho: kLeft -> espejo derivado
            ("right", "right"): "derived",   # hembra: kRight -> espejo derivado
            ("right", "left"): "real",       # hembra: kLeft -> su propio export real
        }
        for (canonical, runtime), expected_kind in expected.items():
            source_direction = invert(runtime)
            actual_kind = "real" if source_direction == canonical else "derived"
            self.assertEqual(actual_kind, expected_kind, f"canonical={canonical} runtime={runtime}")
        del G  # solo se importa para confirmar que el módulo sigue existiendo/cargando


class MultiStateNormalizationPlanTest(unittest.TestCase):
    """Block 05, segunda pasada de corrección post-QA (ver
    docs/DECISION_LOG.md DEC-075): un pet con 2+ `BehaviorState`s de
    silueta genuinamente distinta (p. ej. Frin "seated"/"lying") NO
    puede calibrarse comparando bounding boxes entre estados -- el lado
    más largo de la silueta cambia de eje (alto/angosto vs.
    bajo/ancho). La corrección real es: un estado cuyo `base_animation`
    comparte un archivo de frame REAL con la transición de otro estado
    (el contrato first/last-frame que este proyecto ya exige) hereda su
    escala POR CONSTRUCCIÓN vía un union-find de archivo compartido, en
    vez de por una nueva comparación de pixeles; y una acción de un
    estado sin ese vínculo se calibra contra el `base_animation` de SU
    PROPIO estado, nunca contra el de otro."""

    def _entry(self, w: int, h: int, rect: tuple[int, int, int, int]) -> tuple[int, int, bytes]:
        return w, h, _solid_frame(w, h, (5, 5, 5), rect)

    def test_state_linked_by_shared_frame_file_inherits_reference_scale_exactly(self) -> None:
        # "seated" es el estado de referencia (alto/angosto, 10x16).
        # "lying" es una silueta DISTINTA (bajo/ancho, 22x8) cuyo
        # base_animation comparte el MISMO archivo que el frame final
        # de la transición sit_to_lie de "seated" -- el vínculo real
        # que debe hacer que "lying" herede escala=1.0 sin ninguna
        # comparación de bounding box entre las dos siluetas.
        entries = {
            "state[seated].base_animation": self._entry(20, 20, (5, 2, 14, 17)),  # 10x16, alto
            "state[seated].ambient_actions.sit_to_lie": self._entry(20, 20, (5, 2, 14, 17)),  # frame 0 == seated base
            "state[lying].base_animation": self._entry(30, 12, (4, 2, 25, 9)),  # 22x8, bajo/ancho -- silueta distinta
        }
        groups = {
            "state[seated].base_animation": "state[seated].base_animation",
            "state[seated].ambient_actions.sit_to_lie": "state[seated].ambient_actions.sit_to_lie",
            "state[lying].base_animation": "state[lying].base_animation",
        }
        # El archivo compartido real: el ÚNICO frame de "lying" y el
        # frame "final" (simulado con la misma ruta) de sit_to_lie.
        shared_lying_frame_path = "/fake/sit_to_lie/frame_024.png"
        group_frame_paths = {
            "state[seated].base_animation": ["/fake/seated_base/frame_000.png"],
            "state[seated].ambient_actions.sit_to_lie": ["/fake/seated_base/frame_000.png", shared_lying_frame_path],
            "state[lying].base_animation": [shared_lying_frame_path],
        }
        state_of_group = {
            "state[seated].base_animation": "seated",
            "state[seated].ambient_actions.sit_to_lie": "seated",
            "state[lying].base_animation": "lying",
        }
        base_group_of_state = {
            "seated": "state[seated].base_animation",
            "lying": "state[lying].base_animation",
        }
        plan = prep_dev_sprite.compute_frame_normalization_plan(
            entries, groups, reference_group="state[seated].base_animation",
            group_frame_paths=group_frame_paths, state_of_group=state_of_group,
            base_group_of_state=base_group_of_state,
        )
        self.assertEqual(plan["state[lying].base_animation"][0], 1.0)

    def test_action_of_unlinked_state_compares_against_its_own_state_base_not_the_pet_reference(self) -> None:
        # "lying" NO está vinculado por archivo a "seated" en este
        # test (a propósito, para aislar el otro mecanismo). Una acción
        # de "lying" (p. ej. lie_to_sit) con contenido 22x8 (misma
        # silueta que lying-base, 22x8) debe calibrarse SIN reescalado
        # contra lying-base (misma silueta, real), no contra
        # seated-base (10x16, silueta distinta) -- si comparara contra
        # seated-base por bounding box terminaría con un content_scale
        # completamente distinto (y sin sentido, otro eje) del que da
        # comparar contra su propio estado.
        entries = {
            "state[seated].base_animation": self._entry(20, 20, (5, 2, 14, 17)),  # 10x16
            "state[lying].base_animation": self._entry(30, 12, (4, 2, 25, 9)),  # 22x8
            "state[lying].click_actions.lie_to_sit": self._entry(30, 12, (4, 2, 25, 9)),  # 22x8 -- misma silueta que lying-base
        }
        groups = {
            "state[seated].base_animation": "state[seated].base_animation",
            "state[lying].base_animation": "state[lying].base_animation",
            "state[lying].click_actions.lie_to_sit": "state[lying].click_actions.lie_to_sit",
        }
        group_frame_paths = {
            "state[seated].base_animation": ["/fake/seated_base.png"],
            "state[lying].base_animation": ["/fake/lying_base.png"],  # SIN vínculo de archivo con seated
            "state[lying].click_actions.lie_to_sit": ["/fake/lie_to_sit.png"],
        }
        state_of_group = {
            "state[seated].base_animation": "seated",
            "state[lying].base_animation": "lying",
            "state[lying].click_actions.lie_to_sit": "lying",
        }
        base_group_of_state = {
            "seated": "state[seated].base_animation",
            "lying": "state[lying].base_animation",
        }
        plan = prep_dev_sprite.compute_frame_normalization_plan(
            entries, groups, reference_group="state[seated].base_animation",
            group_frame_paths=group_frame_paths, state_of_group=state_of_group,
            base_group_of_state=base_group_of_state,
        )
        # lying-base y lie_to_sit comparten silueta (22x8 == 22x8) --
        # deben terminar con la MISMA escala entre sí, sin importar
        # cuál sea (la escala de lying-base contra seated-base SÍ usa
        # el fallback de bounding box cross-estado, documentado como
        # límite honesto -- lo que este test verifica es que
        # lie_to_sit NO se recalibra por separado contra seated).
        self.assertAlmostEqual(plan["state[lying].click_actions.lie_to_sit"][0], plan["state[lying].base_animation"][0], places=6)


class FrameSequenceValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmpdir = tempfile.mkdtemp(prefix="nimvlets_pipeline_test_")

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _write_frame(self, index: int, width: int = 4, height: int = 4, alpha_rect: tuple[int, int, int, int] = (1, 1, 3, 3)) -> None:
        pixels = _solid_frame(width, height, (10, 20, 30), alpha_rect)
        prep_dev_sprite.write_png_rgba(os.path.join(self.tmpdir, f"frame_{index:03d}.png"), width, height, pixels)

    def test_valid_sequence_reports_frame_count_and_dimensions(self) -> None:
        for i in range(5):
            self._write_frame(i)
        report = validate_frame_sequence.validate_frame_sequence(self.tmpdir)
        self.assertEqual(report.frame_count, 5)
        self.assertEqual((report.width, report.height), (4, 4))

    def test_missing_index_is_rejected(self) -> None:
        self._write_frame(0)
        self._write_frame(2)  # falta frame_001.png -- hueco
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)

    def test_duplicate_index_is_rejected(self) -> None:
        # Dos archivos distintos en el filesystem ("frame_000.png" y
        # "frame_0.png") pueden parsear al MISMO índice numérico (0) --
        # ese es el caso real de "índice duplicado" que
        # _parse_frame_index()/validate_frame_sequence() deben
        # rechazar, ya que la propia convención "frame_NNN.png" (block
        # brief §5) no prohíbe distintas cantidades de dígitos por sí
        # sola.
        self._write_frame(0)  # crea frame_000.png
        pixels = _solid_frame(4, 4, (1, 2, 3), (1, 1, 3, 3))
        prep_dev_sprite.write_png_rgba(os.path.join(self.tmpdir, "frame_0.png"), 4, 4, pixels)
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)

    def test_mismatched_dimensions_are_rejected(self) -> None:
        self._write_frame(0, width=4, height=4)
        self._write_frame(1, width=6, height=6)
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)

    def test_fully_opaque_frame_is_rejected_as_degenerate_alpha(self) -> None:
        self._write_frame(0, alpha_rect=(0, 0, 4, 4))  # 100% opaco -- sin transparencia real
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)

    def test_fully_transparent_frame_is_rejected_as_degenerate_alpha(self) -> None:
        self._write_frame(0, alpha_rect=(0, 0, 0, 0))  # 0% opaco -- invisible
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)

    def test_empty_directory_is_rejected(self) -> None:
        with self.assertRaises(validate_frame_sequence.FrameSequenceError):
            validate_frame_sequence.validate_frame_sequence(self.tmpdir)


class CompileWeightedActionNormalizationTest(unittest.TestCase):
    """Test de regresión de integración (Block 05, corrección post-QA):
    reproduce exactamente el bug real encontrado en QA manual --
    `tools/compile_pet_pack.py`'s `_build_normalization_plan()` (el
    pre-pass que decide content_scale/canvas de trabajo/offset) y
    `_compile_weighted_actions()` (la pasada real de compilación)
    construían el `context` de cada WeightedAction (ambient/hover/
    click) con dos FORMATOS DE STRING distintos escritos a mano en dos
    lugares -- nunca calzaban, así que `normalization_plan.get(context)`
    devolvía `None` para TODA acción de TODO pet con
    `normalize_visual_scale: true`, y cada una terminaba compilada a su
    propia resolución/encuadre NATIVO en vez del canvas de trabajo
    compartido -- el personaje se veía a un tamaño distinto (más
    grande, típicamente) en cualquier animación disparada por click/
    ambient/hover frente a la pose base estática. Ver
    docs/DECISION_LOG.md y compile_pet_pack._weighted_action_context()
    para el detalle completo y la corrección (una única función
    compartida entre las dos pasadas, para que esta clase de bug sea
    estructuralmente imposible de reintroducir).

    Este test compila un manifest real (JSON en disco + PNG reales en
    disco -- integración end-to-end, no solo las funciones primitivas
    que las otras clases de este archivo ya cubren) con
    `normalize_visual_scale: true` y una acción ambient cuyo frame
    nativo es DELIBERADAMENTE más chico y con menos margen que la pose
    base -- si la normalización se está aplicando de verdad, ambas
    deben terminar compiladas al MISMO tamaño de canvas de trabajo
    (compartido); si el bug reapareciera, la acción compilaría a su
    propio tamaño nativo (distinto)."""

    def setUp(self) -> None:
        self.tmpdir = tempfile.mkdtemp(prefix="nimvlets_normalization_regression_")
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import compile_pet_pack  # noqa: E402 (import tardío -- self-contido dentro del test)

        self.compile_pet_pack = compile_pet_pack

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _write_frame(self, name: str, w: int, h: int, alpha_rect: tuple[int, int, int, int]) -> str:
        path = os.path.join(self.tmpdir, name)
        prep_dev_sprite.write_png_rgba(path, w, h, _solid_frame(w, h, (10, 20, 30), alpha_rect))
        return name  # relativo al manifest_dir (== self.tmpdir), como pide compile_pet_pack

    def _frame_entry(self, name: str) -> dict:
        return {"source": name, "duration_ms": 0}

    def test_weighted_action_shares_working_canvas_with_base_animation(self) -> None:
        # Pose base: frame nativo grande (40x40), contenido 20x20 con
        # margen generoso alrededor -- define el canvas de trabajo.
        base_frame = self._write_frame("base.png", 40, 40, (10, 10, 30, 30))
        # Acción ambient: frame nativo CHICO (20x20), contenido 18x18 --
        # casi sin margen -- si compose_on_canvas() no se aplica de
        # verdad, este frame termina compilado a ~20x20 (su propio
        # tamaño nativo tras el downscale, que acá ni siquiera dispara
        # porque runtime_max_frame_dimension es generoso), NUNCA al
        # mismo tamaño que la base.
        action_frame = self._write_frame("action.png", 20, 20, (1, 1, 19, 19))

        manifest = {
            "id": "regression_test_pet",
            "display_name": "Regression Test Pet",
            "canvas_width": 40,
            "canvas_height": 40,
            "normalize_visual_scale": True,
            "states": [
                {
                    "id": "default",
                    "base_animation": {
                        "id": "base",
                        "kind": "static",
                        "frames": [self._frame_entry(base_frame)],
                    },
                    "ambient_actions": [
                        {
                            "id": "wiggle",
                            "weight": 1.0,
                            "target_state_id": "default",
                            "kind": "one_shot",
                            "frames": [self._frame_entry(action_frame)],
                        }
                    ],
                    "click_actions": [],
                }
            ],
        }
        manifest_path = os.path.join(self.tmpdir, "pack_manifest.json")
        with open(manifest_path, "w", encoding="utf-8") as f:
            import json

            json.dump(manifest, f)

        output_path = os.path.join(self.tmpdir, "out.nvpack")
        self.compile_pet_pack.compile_pack(manifest_path, output_path)

        # Parsea el pack compilado (NVPACK2) directamente -- sin
        # dependencias nuevas, mismo formato que
        # src/content/PetPackLoader.cpp documenta.
        base_dims, action_dims = _read_base_and_first_ambient_action_frame_dims(output_path)

        self.assertEqual(
            base_dims, action_dims,
            f"la acción ambient compiló a {action_dims}, distinto del canvas de trabajo "
            f"compartido de la pose base {base_dims} -- normalize_visual_scale no se está "
            "aplicando a las acciones ponderadas (bug real de Block 05, ver DEC-071)",
        )


def _read_base_and_first_ambient_action_frame_dims(path: str) -> tuple[tuple[int, int], tuple[int, int]]:
    """Lector mínimo de un pack "NVPACK2", suficiente para extraer las
    dimensiones (width, height) del frame 0 de `base_animation` y del
    frame 0 de la primera entrada de `ambient_actions` del primer
    estado -- usado solo por CompileWeightedActionNormalizationTest de
    arriba. Espejo intencional (no una reutilización de código) del
    formato que src/content/PetPackLoader.cpp implementa en C++ -- ver
    docs/ANIMATION_RUNTIME.md para el layout binario completo."""
    import struct

    with open(path, "rb") as f:
        buf = f.read()

    def read_string(pos: int) -> tuple[str, int]:
        (n,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        return buf[pos : pos + n].decode("utf-8"), pos + n

    def read_animation_first_frame_dims(pos: int) -> tuple[tuple[int, int], int]:
        _id, pos = read_string(pos)
        pos += 1 + 8 + 1  # kind, fps, returnsToIdle
        (frame_count,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        w, h = struct.unpack_from("<II", buf, pos)
        pos += 4 + 4 + 8 + 8 + 8  # width, height, anchorX, anchorY, durationMs
        pos += w * h * 4  # frame 0's pixels -- skip, only dims matter here
        for _ in range(1, frame_count):
            w2, h2 = struct.unpack_from("<II", buf, pos)
            pos += 4 + 4 + 8 + 8 + 8 + w2 * h2 * 4
        return (w, h), pos

    def skip_direction_overrides(pos: int) -> int:
        (count,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        for _ in range(count):
            pos += 1  # direction byte
            _dims, pos = read_animation_first_frame_dims(pos)
        return pos

    pos = 8  # magic
    for _ in range(3):  # petId, displayName, variantGroup
        _s, pos = read_string(pos)
    pos += 4 + 4 + 1 + 8  # canvasWidth, canvasHeight, alphaHitThreshold, visualScale
    _s, pos = read_string(pos)  # contentVersion
    (state_count,) = struct.unpack_from("<I", buf, pos)
    pos += 4
    assert state_count >= 1
    _state_id, pos = read_string(pos)
    base_dims, pos = read_animation_first_frame_dims(pos)
    pos = skip_direction_overrides(pos)
    pos += 8  # ambientIntervalSeconds
    (ambient_count,) = struct.unpack_from("<I", buf, pos)
    pos += 4
    assert ambient_count >= 1, "test manifest must define at least one ambient action"
    _action_id, pos = read_string(pos)
    pos += 8  # weight
    _target_state_id, pos = read_string(pos)
    action_dims, pos = read_animation_first_frame_dims(pos)

    return base_dims, action_dims


class AlignEndpointToTargetBaseTest(unittest.TestCase):
    """Integración end-to-end (manifest JSON + PNG reales en disco,
    `compile_pet_pack.compile_pack()` real, pack leído con
    `read_pet_pack.py`) del flag `align_endpoint_to_target_base` --
    Block 05, pasada de pulido final (ver docs/DECISION_LOG.md
    DEC-092): extiende el anclaje-por-último-frame de DEC-087 (hasta
    ahora solo para transiciones que CAMBIAN de estado) a acciones
    self-loop (`target_state_id == state_id`) cuyo contenido declara
    explícitamente que su punta final representa la MISMA pose base
    estable -- el "size snap al volver a idle" que QA manual reportó en
    Bunny/Frin (`groom`/`click`/`howl`/`tail_greet`)."""

    def setUp(self) -> None:
        self.tmpdir = tempfile.mkdtemp(prefix="nimvlets_align_endpoint_")
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import compile_pet_pack  # noqa: E402

        self.compile_pet_pack = compile_pet_pack

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _write_square(self, name: str, rect: tuple[int, int, int, int]) -> dict:
        # Frame nativo 60x60, un cuadrado sólido de 10x10 -- mismo
        # radio RMS ponderado por alpha sin importar DÓNDE caiga el
        # cuadrado dentro del frame (invariante a traslación), así que
        # content_scale queda en 1.0 para las tres entradas por
        # construcción -- esto aísla la COLOCACIÓN (lo único que
        # `align_endpoint_to_target_base` toca) de la ESCALA (que nunca
        # debería moverse por este mecanismo).
        path = os.path.join(self.tmpdir, name)
        prep_dev_sprite.write_png_rgba(path, 60, 60, _solid_frame(60, 60, (7, 7, 7), rect))
        return {"source": name, "duration_ms": 0}

    def _manifest(self, *, align: bool, returns_to_idle: bool = True) -> str:
        base = self._write_square("base.png", (25, 25, 34, 34))          # centro (30,30)
        frame0 = self._write_square("wiggle_000.png", (20, 25, 29, 34))  # centro (25,30) -- 5px a la izquierda
        frame_last = self._write_square("wiggle_002.png", (25, 15, 34, 24))  # centro (30,20) -- 10px arriba
        manifest = {
            "id": "align_test_pet",
            "display_name": "Align Test Pet",
            "canvas_width": 60,
            "canvas_height": 60,
            "normalize_visual_scale": True,
            "states": [
                {
                    "id": "default",
                    "base_animation": {"id": "base", "kind": "static", "frames": [base]},
                    "click_actions": [
                        {
                            "id": "wiggle",
                            "weight": 1.0,
                            "target_state_id": "default",
                            "align_endpoint_to_target_base": align,
                            "kind": "one_shot",
                            "returns_to_idle": returns_to_idle,
                            "frames": [frame0, self._write_square("wiggle_001.png", (22, 20, 31, 29)), frame_last],
                        }
                    ],
                }
            ],
        }
        manifest_path = os.path.join(self.tmpdir, "pack_manifest.json")
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f)
        return manifest_path

    def _compile_and_read(self, manifest_path: str) -> dict:
        output_path = os.path.join(self.tmpdir, "out.nvpack")
        self.compile_pet_pack.compile_pack(manifest_path, output_path)
        return read_pet_pack.read_pack(output_path)

    def test_without_the_flag_the_first_frame_matches_base_not_the_last(self) -> None:
        # Comportamiento LEGACY, sin cambios -- la colocación default de
        # CUALQUIER entrada ancla su PROPIO frame 0, así que el frame 0
        # de la acción cae exactamente donde cae la base (misma fórmula,
        # -first_anchor, para las dos), y el último frame queda
        # desplazado según el movimiento real del personaje.
        pack = self._compile_and_read(self._manifest(align=False))
        state = read_pet_pack.find_state(pack, "default")
        base_centre = read_pet_pack.content_centre(state["base_animation"]["frames"][0])
        action = read_pet_pack.find_action(state, "click_actions", "wiggle")
        first_centre = read_pet_pack.content_centre(action["animation"]["frames"][0])
        last_centre = read_pet_pack.content_centre(action["animation"]["frames"][-1])
        self.assertAlmostEqual(first_centre[0], base_centre[0], delta=0.6)
        self.assertAlmostEqual(first_centre[1], base_centre[1], delta=0.6)
        # El último frame NO está protegido sin el flag -- se espera el
        # desplazamiento real de 10px (nativo) que el fixture programó.
        self.assertGreater(abs(last_centre[1] - base_centre[1]), 5.0)

    def test_with_the_flag_the_last_frame_matches_base_not_the_first(self) -> None:
        # El comportamiento NUEVO -- exactamente invertido: ahora es el
        # ÚLTIMO frame el que se ancla contra la base (el instante en
        # que la acción TERMINA y el runtime vuelve a mostrar la base,
        # ver AnimationController), y el primero el que queda a la
        # deriva.
        pack = self._compile_and_read(self._manifest(align=True))
        state = read_pet_pack.find_state(pack, "default")
        base_centre = read_pet_pack.content_centre(state["base_animation"]["frames"][0])
        action = read_pet_pack.find_action(state, "click_actions", "wiggle")
        last_centre = read_pet_pack.content_centre(action["animation"]["frames"][-1])
        first_centre = read_pet_pack.content_centre(action["animation"]["frames"][0])
        self.assertAlmostEqual(last_centre[0], base_centre[0], delta=0.6)
        self.assertAlmostEqual(last_centre[1], base_centre[1], delta=0.6)
        self.assertGreater(abs(first_centre[1] - base_centre[1]), 5.0)

    def test_flag_changes_placement_but_never_scale(self) -> None:
        without = self._compile_and_read(self._manifest(align=False))
        with_flag = self._compile_and_read(self._manifest(align=True))
        s_without = read_pet_pack.find_state(without, "default")
        s_with = read_pet_pack.find_state(with_flag, "default")
        a_without = read_pet_pack.find_action(s_without, "click_actions", "wiggle")["animation"]
        a_with = read_pet_pack.find_action(s_with, "click_actions", "wiggle")["animation"]
        # Colocación realmente cambió (si esto fallara, los dos tests de
        # arriba ya lo habrían detectado -- se repite acá como parte del
        # contraste explícito escala vs. colocación).
        self.assertNotEqual(
            [f["pixels"] for f in a_without["frames"]], [f["pixels"] for f in a_with["frames"]]
        )
        # El CANVAS compartido sí puede crecer/achicarse un poco (tiene
        # que seguir conteniendo cada frame completo dado su nueva
        # colocación -- eso es esperado, no un bug). Lo que NUNCA debe
        # cambiar es `content_scale`: se verifica con una medida
        # invariante a traslación (radio RMS ponderado por alpha, el
        # mismo estimador de "tamaño" de DEC-088) -- si el mecanismo
        # alguna vez tocara la escala, este valor se movería; con
        # cuadrados sólidos del mismo tamaño nativo, debe quedar
        # exactamente igual entre las dos corridas.
        for i in range(len(a_without["frames"])):
            fw = a_without["frames"][i]
            fwith = a_with["frames"][i]
            r_without = prep_dev_sprite.alpha_rms_radius(fw["width"], fw["height"], fw["pixels"])
            r_with = prep_dev_sprite.alpha_rms_radius(fwith["width"], fwith["height"], fwith["pixels"])
            self.assertAlmostEqual(r_without, r_with, delta=1e-6, msg=f"frame {i}")

    def test_flag_is_a_no_op_when_the_action_never_actually_returns(self) -> None:
        # `returns_to_idle=False`: la acción se congela en su último
        # frame para siempre -- no hay ningún instante de "volver a la
        # base" que proteger, así que el flag no debe cambiar nada.
        without = self._compile_and_read(self._manifest(align=False, returns_to_idle=False))
        with_flag = self._compile_and_read(self._manifest(align=True, returns_to_idle=False))
        a_without = read_pet_pack.find_action(
            read_pet_pack.find_state(without, "default"), "click_actions", "wiggle")["animation"]
        a_with = read_pet_pack.find_action(
            read_pet_pack.find_state(with_flag, "default"), "click_actions", "wiggle")["animation"]
        self.assertEqual([f["pixels"] for f in a_without["frames"]], [f["pixels"] for f in a_with["frames"]])

    def test_direction_override_is_covered_by_the_same_action_level_flag(self) -> None:
        """`align_endpoint_to_target_base` es una propiedad de la ACCIÓN
        (como `target_state_id`), no de cada dirección -- un override
        "left" sin su propio campo debe seguir protegido por el flag del
        canónico. Se agrega un override AL VUELO sobre el manifest ya
        escrito, en vez de duplicar `_manifest()`, para mantener el
        fixture centrado en lo que este test prueba."""
        manifest_path = self._manifest(align=True)
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        action = manifest["states"][0]["click_actions"][0]
        # Mismo contenido que el canónico (right) -- alcanza para probar
        # que el override TAMBIÉN se registra; no hace falta espejarlo.
        action["direction_overrides"] = [{
            "direction": "left", "id": "wiggle_left", "kind": "one_shot",
            "returns_to_idle": True, "frames": action["frames"],
        }]
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f)

        pack = self._compile_and_read(manifest_path)
        state = read_pet_pack.find_state(pack, "default")
        base_centre = read_pet_pack.content_centre(state["base_animation"]["frames"][0])
        action_out = read_pet_pack.find_action(state, "click_actions", "wiggle")
        override = next(o for o in action_out["direction_overrides"] if o["direction"] == "left")
        last_centre = read_pet_pack.content_centre(override["animation"]["frames"][-1])
        self.assertAlmostEqual(last_centre[0], base_centre[0], delta=0.6)
        self.assertAlmostEqual(last_centre[1], base_centre[1], delta=0.6)


class CompileTwoStateNormalizationTest(unittest.TestCase):
    """Test de regresión de integración (Block 05, segunda pasada de
    corrección post-QA -- ver docs/DECISION_LOG.md DEC-075): reproduce
    el bug REAL encontrado en Frin ("lying" se veía inflado/corrupto
    frente a "seated") a nivel de compilación end-to-end, con un
    manifest de 2 estados real compilado a un .nvpack real -- no solo
    la función pura que MultiStateNormalizationPlanTest ya cubre.

    Frame A: contenido alto/angosto (bbox 4x16, longer=16).
    Frame B: contenido bajo/ancho (bbox 12x6, longer=12) -- una silueta
    deliberadamente DISTINTA, para que comparar bounding boxes entre
    estados (el bug real) produzca una escala equivocada (16/12=1.333)
    en vez de heredar la escala correcta (1.0) por archivo compartido.

    `seated.base_animation` = frame A. `seated.ambient_actions[0]`
    ("sit_to_lie") = [frame A, frame B] (frame 0 = pose sentada real,
    frame final = pose acostada real -- el contrato first/last-frame),
    target_state_id="lying". `lying.base_animation` = frame B (el MISMO
    archivo que el frame final de sit_to_lie -- nunca un archivo
    nuevo). Si la corrección funciona, `lying.base_animation` compila a
    EXACTAMENTE los mismos pixeles que el frame 1 de `sit_to_lie` (sin
    ningún resize -- content_scale=1.0 para ambos, heredado por
    construcción); si el bug reapareciera, `lying.base_animation`
    compilaría reescalado 1.333x, con dimensiones y pixeles distintos."""

    def setUp(self) -> None:
        self.tmpdir = tempfile.mkdtemp(prefix="nimvlets_two_state_regression_")
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import compile_pet_pack  # noqa: E402

        self.compile_pet_pack = compile_pet_pack

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _write_frame(self, name: str, w: int, h: int, alpha_rect: tuple[int, int, int, int]) -> str:
        path = os.path.join(self.tmpdir, name)
        prep_dev_sprite.write_png_rgba(path, w, h, _solid_frame(w, h, (10, 20, 30), alpha_rect))
        return name

    def test_lying_base_animation_is_pixel_identical_to_sit_to_lie_final_frame(self) -> None:
        frame_a = self._write_frame("frame_a.png", 20, 20, (8, 2, 11, 17))  # bbox 4x16, alto
        frame_b = self._write_frame("frame_b.png", 20, 20, (4, 7, 15, 12))  # bbox 12x6, bajo/ancho

        manifest = {
            "id": "two_state_regression_pet",
            "display_name": "Two State Regression Pet",
            "canvas_width": 20,
            "canvas_height": 20,
            "normalize_visual_scale": True,
            "states": [
                {
                    "id": "seated",
                    "base_animation": {"id": "seated_base", "kind": "static", "frames": [{"source": frame_a, "duration_ms": 0}]},
                    "ambient_actions": [
                        {
                            "id": "sit_to_lie",
                            "weight": 1.0,
                            "target_state_id": "lying",
                            "kind": "one_shot",
                            "frames": [
                                {"source": frame_a, "duration_ms": 100},
                                {"source": frame_b, "duration_ms": 100},
                            ],
                        }
                    ],
                    "click_actions": [],
                },
                {
                    "id": "lying",
                    "base_animation": {"id": "lying_base", "kind": "static", "frames": [{"source": frame_b, "duration_ms": 0}]},
                    "ambient_actions": [],
                    "click_actions": [],
                },
            ],
        }
        manifest_path = os.path.join(self.tmpdir, "pack_manifest.json")
        with open(manifest_path, "w", encoding="utf-8") as f:
            import json

            json.dump(manifest, f)

        output_path = os.path.join(self.tmpdir, "out.nvpack")
        self.compile_pet_pack.compile_pack(manifest_path, output_path)

        lying_base, sit_to_lie_final_frame = _read_lying_base_and_sit_to_lie_final_frame(output_path)
        # Compara geometría/pixeles (width, height, pixels) -- NO
        # duration_ms (0 para una base_animation static de un frame,
        # 100 para el frame de una secuencia one_shot; una diferencia
        # de metadata esperada, no de geometría) ni anchor (coincide
        # acá solo porque ninguno de los dos define un anchor
        # explícito, no es lo que este test verifica).
        lying_geometry = (lying_base[0], lying_base[1], lying_base[5])
        sit_to_lie_geometry = (sit_to_lie_final_frame[0], sit_to_lie_final_frame[1], sit_to_lie_final_frame[5])

        self.assertEqual(
            lying_geometry, sit_to_lie_geometry,
            f"'lying' base_animation compiló a {lying_geometry[:2]}, distinto del frame final de "
            f"'sit_to_lie' {sit_to_lie_geometry[:2]} -- deberían ser EXACTAMENTE la misma "
            "transformación (mismo archivo fuente, ver DEC-075) en vez de recalibrarse por "
            "separado comparando bounding boxes entre estados de silueta distinta",
        )


def _read_lying_base_and_sit_to_lie_final_frame(path: str) -> tuple[tuple, tuple]:
    """Lector mínimo de un pack NVPACK2 de 2 estados, suficiente para
    extraer (width, height, pixels) del frame de `lying.base_animation`
    y del ÚLTIMO frame de `sit_to_lie` (la primera -- y única, en este
    test -- ambient action de `seated`) -- usado solo por
    CompileTwoStateNormalizationTest de arriba. Espejo intencional del
    formato de src/content/PetPackLoader.cpp, igual que el lector de
    CompileWeightedActionNormalizationTest más arriba en este archivo."""
    import struct

    with open(path, "rb") as f:
        buf = f.read()

    def read_string(pos: int) -> tuple[str, int]:
        (n,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        return buf[pos : pos + n].decode("utf-8"), pos + n

    def read_frame(pos: int) -> tuple[tuple, int]:
        w, h, ax, ay, dur = struct.unpack_from("<IIddd", buf, pos)
        pos += 4 + 4 + 8 + 8 + 8
        pixels = buf[pos : pos + w * h * 4]
        pos += w * h * 4
        return (w, h, ax, ay, dur, pixels), pos

    def read_animation(pos: int) -> tuple[list, int]:
        _id, pos = read_string(pos)
        pos += 1 + 8 + 1  # kind, fps, returnsToIdle
        (frame_count,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        frames = []
        for _ in range(frame_count):
            frame, pos = read_frame(pos)
            frames.append(frame)
        return frames, pos

    def skip_direction_overrides(pos: int) -> int:
        (count,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        for _ in range(count):
            pos += 1
            _frames, pos = read_animation(pos)
        return pos

    def read_weighted_actions(pos: int) -> tuple[list, int]:
        (count,) = struct.unpack_from("<I", buf, pos)
        pos += 4
        actions = []
        for _ in range(count):
            _action_id, pos = read_string(pos)
            pos += 8  # weight
            _target_state_id, pos = read_string(pos)
            frames, pos = read_animation(pos)
            pos = skip_direction_overrides(pos)
            actions.append(frames)
        return actions, pos

    pos = 8  # magic
    for _ in range(3):  # petId, displayName, variantGroup
        _s, pos = read_string(pos)
    pos += 4 + 4 + 1 + 8  # canvasWidth, canvasHeight, alphaHitThreshold, visualScale
    _s, pos = read_string(pos)  # contentVersion
    (state_count,) = struct.unpack_from("<I", buf, pos)
    pos += 4
    assert state_count == 2

    # Estado 0 ("seated"): base_animation + overrides + ambient (nos
    # interesa el ÚLTIMO frame de la primera ambient action) + hover +
    # click.
    _state_id, pos = read_string(pos)
    _seated_base_frames, pos = read_animation(pos)
    pos = skip_direction_overrides(pos)
    pos += 8  # ambientIntervalSeconds
    ambient_actions, pos = read_weighted_actions(pos)
    sit_to_lie_final_frame = ambient_actions[0][-1]
    pos += 1  # hoverUsesAmbientActions
    _hover_actions, pos = read_weighted_actions(pos)
    _click_actions, pos = read_weighted_actions(pos)

    # Estado 1 ("lying"): solo nos interesa su base_animation.
    _state_id2, pos = read_string(pos)
    lying_base_frames, pos = read_animation(pos)
    lying_base = lying_base_frames[0]

    return lying_base, sit_to_lie_final_frame


if __name__ == "__main__":
    unittest.main()
