#!/usr/bin/env python3
"""Tests puros (stdlib `unittest`, sin dependencias de terceros) para
la lógica de tools/ que Block 04.2 agrega: espejado horizontal
(`prep_dev_sprite.mirror_rgba_horizontal`), validación de secuencias
de frames (`validate_frame_sequence.validate_frame_sequence`), y --
agregado en la segunda pasada de este bloque -- la política genérica
de canvas lógico (`prep_dev_sprite.compute_logical_canvas_size`) y el
downscale opcional de runtime (`prep_dev_sprite.resize_rgba_nearest`).

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
