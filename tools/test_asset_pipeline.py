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

    def test_bunny_reference_size_is_unchanged(self) -> None:
        # Sanity check explícito: la convención ya establecida de Bunny
        # (160x160, cuadrado) debe seguir devolviendo exactamente
        # 160x160 -- si esto cambiara, Bunny se vería afectado.
        self.assertEqual(prep_dev_sprite.compute_logical_canvas_size(160, 160), (160, 160))

    def test_wide_native_resolution_scales_longer_edge_to_reference(self) -> None:
        # 513x525 (nativo real de Nidir) -> el lado más largo (525)
        # queda en 160, el otro se escala proporcionalmente.
        canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        self.assertEqual(canvas_height, 160)
        self.assertEqual(canvas_width, 156)

    def test_aspect_ratio_is_preserved_within_rounding(self) -> None:
        native_width, native_height = 800, 400
        canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(native_width, native_height)
        native_ratio = native_width / native_height
        canvas_ratio = canvas_width / canvas_height
        self.assertAlmostEqual(native_ratio, canvas_ratio, delta=0.01)

    def test_custom_reference_size_is_honored(self) -> None:
        self.assertEqual(prep_dev_sprite.compute_logical_canvas_size(200, 100, reference_size=100), (100, 50))

    def test_result_is_deterministic(self) -> None:
        first = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        second = prep_dev_sprite.compute_logical_canvas_size(513, 525)
        self.assertEqual(first, second)

    def test_rejects_non_positive_dimensions(self) -> None:
        with self.assertRaises(ValueError):
            prep_dev_sprite.compute_logical_canvas_size(0, 100)
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
    docs/NIDIR_CONTENT.md)."""

    def _entry(self, w: int, h: int, rect: tuple[int, int, int, int]) -> tuple[int, int, bytes]:
        return w, h, _solid_frame(w, h, (5, 5, 5), rect)

    def test_reference_group_always_gets_scale_one(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(20, 20, (5, 5, 15, 15)),
        }
        groups = {"idle": "idle", "click": "click"}
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
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
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        self.assertAlmostEqual(plan["click"][0], 1.0, places=6)

    def test_different_content_size_yields_real_rescale(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),  # contenido 10x10
            "click": self._entry(20, 20, (8, 8, 12, 12)),  # contenido 4x4 -- mucho más chico
        }
        groups = {"idle": "idle", "click": "click"}
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
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
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        working_dims = {(w, h) for _, w, h, _, _ in plan.values()}
        self.assertEqual(len(working_dims), 1, f"expected one shared working canvas, got {working_dims}")

    def test_canonical_and_override_in_same_group_share_content_scale(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(20, 20, (8, 8, 12, 12)),
            "click_left": self._entry(20, 20, (8, 8, 12, 12)),
        }
        groups = {"idle": "idle", "click": "click", "click_left": "click"}
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        self.assertEqual(plan["click"][0], plan["click_left"][0])

    def test_working_canvas_is_large_enough_to_avoid_cropping_every_entry(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(40, 12, (2, 2, 38, 10)),  # contenido muy ancho, casi todo el frame
        }
        groups = {"idle": "idle", "click": "click"}
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
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
        plan = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        self.assertIn("click", plan)  # no debe fallar ni omitir la entrada

    def test_rejects_unknown_reference_group(self) -> None:
        entries = {"idle": self._entry(10, 10, (2, 2, 8, 8))}
        groups = {"idle": "idle"}
        with self.assertRaises(ValueError):
            prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="does_not_exist")

    def test_result_is_deterministic(self) -> None:
        entries = {
            "idle": self._entry(20, 20, (5, 5, 15, 15)),
            "click": self._entry(30, 30, (10, 10, 20, 20)),
        }
        groups = {"idle": "idle", "click": "click"}
        first = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        second = prep_dev_sprite.compute_frame_normalization_plan(entries, groups, reference_group="idle")
        self.assertEqual(first, second)


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


if __name__ == "__main__":
    unittest.main()
