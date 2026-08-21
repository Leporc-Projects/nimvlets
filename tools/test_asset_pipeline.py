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

import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prep_dev_sprite  # noqa: E402
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
        self.assertAlmostEqual(content_scale, 10.0 / 4.0, places=6)

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
