#!/usr/bin/env python3
"""Genera el contenido "left" de Nidir a partir de sus frames "right"
reales, ya importados en este repo (ver docs/NIDIR_CONTENT.md), y
compila el pack de runtime completo con el grafo de comportamiento de
un solo estado ("default") que todo pet normal usa (Block 05 -- ver
docs/ANIMATION_RUNTIME.md): pose base estática + click self-loop
(click-fire real) + ambient self-loop ponderado 70/30 (breathing +
wing_stretch) + hover compartiendo el mismo pool ambient -- política de
canvas de trabajo compartido anclado por contenido sin cambios desde
Block 04.3.

Nidir es canónicamente "right" (a diferencia de Bunny, "left") -- su
export real ya viene nombrado así. Este script deriva "left" por
espejado horizontal determinista.

Block 05 (corrección de comportamiento + escala visual, ver el informe
de este bloque):
    - `ambient_interval_seconds` pasa de 10.0 (Block 04.3) a 15.0 --
      política de producto vigente, un único valor para Bunny y Nidir.
    - `visual_scale` (por-pet, runtime -- ver
      content::PetDefinition::visualScale): Nidir sube de 1.10
      (primera pasada) a 1.25 (segunda pasada de corrección post-QA --
      ver docs/DECISION_LOG.md DEC-076). QA manual real encontró que
      Nidir seguía sintiéndose más chico que Bunny incluso con el
      +10% ya aplicado -- medido con evidencia real (bounding box de
      contenido visible del pack compilado, no solo el tamaño del
      canvas transparente): a 1.10, el contenido visible efectivo de
      Nidir era ~136x155pt, ALGO MÁS BAJO que el de Bunny (~114x159pt)
      pese a ser más ancho -- exactamente "feels smaller" pese al
      ajuste anterior. A 1.25: ~154x176pt, claramente por encima de
      Bunny en ambos ejes. Contraste: tools/generate_bunny_pack.py usa
      el default 1.0 ("Bunny's current size is approved", sin cambio,
      es la referencia). Puramente runtime: los PNG fuente y el pack
      compilado no cambian por esto -- solo cuánto se estira al
      dibujar (ver SpikeApp::EffectiveCanvasWidth()/Height()).

Uso:
    python3 tools/generate_nidir_pack.py

Sin dependencias de terceros.
"""

from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compile_pet_pack  # noqa: E402
import prep_dev_sprite  # noqa: E402
import validate_frame_sequence  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NIDIR_ROOT = os.path.join(REPO_ROOT, "assets", "source", "nimvlets", "nidir")
IDLE_ROOT = os.path.join(NIDIR_ROOT, "animations", "idle")
RIGHT_FRAMES_DIR = os.path.join(IDLE_ROOT, "right", "frames")
LEFT_FRAMES_DIR = os.path.join(IDLE_ROOT, "left", "frames")
RIGHT_SPRITESHEET_PATH = os.path.join(IDLE_ROOT, "right", "spritesheet", "spritesheet.png")
LEFT_SPRITESHEET_PATH = os.path.join(IDLE_ROOT, "left", "spritesheet", "spritesheet.png")

CLICK_ROOT = os.path.join(NIDIR_ROOT, "animations", "click_reaction")
CLICK_RIGHT_FRAMES_DIR = os.path.join(CLICK_ROOT, "right", "frames")
CLICK_LEFT_FRAMES_DIR = os.path.join(CLICK_ROOT, "left", "frames")
CLICK_RIGHT_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "right", "spritesheet", "spritesheet.png")
CLICK_LEFT_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "left", "spritesheet", "spritesheet.png")

WING_STRETCH_ROOT = os.path.join(NIDIR_ROOT, "animations", "wing_stretch")
WING_STRETCH_RIGHT_FRAMES_DIR = os.path.join(WING_STRETCH_ROOT, "right", "frames")
WING_STRETCH_LEFT_FRAMES_DIR = os.path.join(WING_STRETCH_ROOT, "left", "frames")
WING_STRETCH_RIGHT_SPRITESHEET_PATH = os.path.join(WING_STRETCH_ROOT, "right", "spritesheet", "spritesheet.png")
WING_STRETCH_LEFT_SPRITESHEET_PATH = os.path.join(WING_STRETCH_ROOT, "left", "spritesheet", "spritesheet.png")

MANIFEST_PATH = os.path.join(NIDIR_ROOT, "pack_manifest.json")
COMPILED_PATH = os.path.join(REPO_ROOT, "assets", "dev", "nidir_pack.nvpack")

RUNTIME_MAX_FRAME_DIMENSION = 2 * prep_dev_sprite.REFERENCE_LOGICAL_SIZE

GRID_COLUMNS = 5

ALPHA_HIT_THRESHOLD = 128

EXPORT_DURATION_SECONDS = 3.0
CLICK_EXPORT_DURATION_SECONDS = 3.0
WING_STRETCH_EXPORT_DURATION_SECONDS = 3.0

# 15 segundos -- ver el docstring del módulo y tools/generate_bunny_pack.py.
AMBIENT_INTERVAL_SECONDS = 15.0

# Selección ponderada 70/30 -- índice 0 = idle periódico (breathing/
# fuego), índice 1 = wing_stretch.
AMBIENT_ACTION_WEIGHTS = [0.7, 0.3]

# Escala visual por-pet (Block 05 -- ver el docstring del módulo y
# content::PetDefinition::visualScale). Segunda pasada de corrección
# post-QA (DEC-076): sube de 1.10 a 1.25 -- QA manual real reportó que
# Nidir seguía sintiéndose más chico que Bunny; medido con el
# bounding box de contenido visible del pack compilado (no solo el
# canvas transparente), 1.25 deja a Nidir claramente por encima de
# Bunny en ambos ejes (~154x176pt vs. ~114x159pt). Trivial de ajustar:
# cambiar este único número y volver a correr este script.
VISUAL_SCALE = 1.25


def _assemble_spritesheet_from_frames(frame_dir: str, frame_count: int, frame_w: int, frame_h: int) -> tuple[int, int, bytes]:
    rows = (frame_count + GRID_COLUMNS - 1) // GRID_COLUMNS
    sheet_w = GRID_COLUMNS * frame_w
    sheet_h = rows * frame_h
    sheet = bytearray(sheet_w * sheet_h * 4)

    for index in range(frame_count):
        _, _, pixels = prep_dev_sprite.read_png_rgba(os.path.join(frame_dir, f"frame_{index:03d}.png"))
        col = index % GRID_COLUMNS
        row = index // GRID_COLUMNS
        dst_x0 = col * frame_w
        dst_y0 = row * frame_h
        for y in range(frame_h):
            src_row_off = y * frame_w * 4
            dst_row_off = ((dst_y0 + y) * sheet_w + dst_x0) * 4
            sheet[dst_row_off : dst_row_off + frame_w * 4] = pixels[src_row_off : src_row_off + frame_w * 4]

    return sheet_w, sheet_h, bytes(sheet)


def _derive_left_direction(
    label: str,
    right_frames_dir: str,
    left_frames_dir: str,
    right_spritesheet_path: str,
    left_spritesheet_path: str,
) -> tuple[validate_frame_sequence.FrameSequenceReport, validate_frame_sequence.FrameSequenceReport]:
    right_report = validate_frame_sequence.validate_frame_sequence(right_frames_dir, ALPHA_HIT_THRESHOLD)
    print(f"{label}: right frames válidos: {right_report.frame_count} @ {right_report.width}x{right_report.height}")

    if not os.path.isfile(right_spritesheet_path):
        raise SystemExit(f"error: falta el spritesheet right importado de {label}: {right_spritesheet_path}")

    os.makedirs(left_frames_dir, exist_ok=True)
    os.makedirs(os.path.dirname(left_spritesheet_path), exist_ok=True)

    for index in range(right_report.frame_count):
        src_path = os.path.join(right_frames_dir, f"frame_{index:03d}.png")
        w, h, pixels = prep_dev_sprite.read_png_rgba(src_path)
        mirrored = prep_dev_sprite.mirror_rgba_horizontal(w, h, pixels)
        dst_path = os.path.join(left_frames_dir, f"frame_{index:03d}.png")
        prep_dev_sprite.write_png_rgba(dst_path, w, h, mirrored)

    left_report = validate_frame_sequence.validate_frame_sequence(left_frames_dir, ALPHA_HIT_THRESHOLD)
    print(f"{label}: left frames generados y validados: {left_report.frame_count} @ {left_report.width}x{left_report.height}")
    assert left_report.frame_count == right_report.frame_count
    assert (left_report.width, left_report.height) == (right_report.width, right_report.height)
    assert left_report.transparent_fraction_range == right_report.transparent_fraction_range, (
        f"{label}: el espejado horizontal no debería alterar la fracción de pixeles transparentes de ningún frame"
    )

    sheet_w, sheet_h, sheet_pixels = _assemble_spritesheet_from_frames(
        left_frames_dir, left_report.frame_count, left_report.width, left_report.height
    )
    prep_dev_sprite.write_png_rgba(left_spritesheet_path, sheet_w, sheet_h, sheet_pixels)
    print(f"{label}: left spritesheet ensamblado: {left_spritesheet_path} ({sheet_w}x{sheet_h})")

    return right_report, left_report


def _frame_entry(rel_dir: str, index: int) -> dict:
    return {"source": os.path.join(rel_dir, f"frame_{index:03d}.png"), "duration_ms": 0}


def main() -> int:
    right_report, left_report = _derive_left_direction(
        "idle", RIGHT_FRAMES_DIR, LEFT_FRAMES_DIR, RIGHT_SPRITESHEET_PATH, LEFT_SPRITESHEET_PATH
    )
    click_right_report, click_left_report = _derive_left_direction(
        "click_reaction", CLICK_RIGHT_FRAMES_DIR, CLICK_LEFT_FRAMES_DIR, CLICK_RIGHT_SPRITESHEET_PATH, CLICK_LEFT_SPRITESHEET_PATH
    )
    wing_stretch_right_report, wing_stretch_left_report = _derive_left_direction(
        "wing_stretch", WING_STRETCH_RIGHT_FRAMES_DIR, WING_STRETCH_LEFT_FRAMES_DIR,
        WING_STRETCH_RIGHT_SPRITESHEET_PATH, WING_STRETCH_LEFT_SPRITESHEET_PATH
    )

    normalization_entries = {
        "idle": prep_dev_sprite.read_png_rgba(os.path.join(RIGHT_FRAMES_DIR, "frame_000.png")),
        "idle_left": prep_dev_sprite.read_png_rgba(os.path.join(LEFT_FRAMES_DIR, "frame_000.png")),
        "click_reaction": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_RIGHT_FRAMES_DIR, "frame_000.png")),
        "click_reaction_left": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_LEFT_FRAMES_DIR, "frame_000.png")),
        "wing_stretch": prep_dev_sprite.read_png_rgba(os.path.join(WING_STRETCH_RIGHT_FRAMES_DIR, "frame_000.png")),
        "wing_stretch_left": prep_dev_sprite.read_png_rgba(os.path.join(WING_STRETCH_LEFT_FRAMES_DIR, "frame_000.png")),
    }
    normalization_groups = {
        "idle": "idle",
        "idle_left": "idle",
        "click_reaction": "click_reaction",
        "click_reaction_left": "click_reaction",
        "wing_stretch": "wing_stretch",
        "wing_stretch_left": "wing_stretch",
    }
    # Nidir tiene un solo BehaviorState ("default") -- ver el comentario
    # equivalente en tools/generate_bunny_pack.py; el union-find de
    # compute_frame_normalization_plan no fusiona nada acá (idle/
    # click_reaction/wing_stretch nunca comparten archivo entre sí),
    # así que el resultado es idéntico al de antes de Block 05, segunda
    # pasada.
    normalization_state_of_group = {"idle": "default", "click_reaction": "default", "wing_stretch": "default"}
    normalization_base_group_of_state = {"default": "idle"}
    normalization_group_frame_paths = {
        "idle": [os.path.realpath(os.path.join(RIGHT_FRAMES_DIR, "frame_000.png"))],
        "click_reaction": [os.path.realpath(os.path.join(CLICK_RIGHT_FRAMES_DIR, "frame_000.png"))],
        "wing_stretch": [os.path.realpath(os.path.join(WING_STRETCH_RIGHT_FRAMES_DIR, "frame_000.png"))],
    }
    normalization_plan = prep_dev_sprite.compute_frame_normalization_plan(
        normalization_entries, normalization_groups, reference_group="idle",
        group_frame_paths=normalization_group_frame_paths, state_of_group=normalization_state_of_group,
        base_group_of_state=normalization_base_group_of_state,
    )
    _, working_width, working_height, _, _ = normalization_plan["idle"]
    print(f"canvas de trabajo compartido (idle + click_reaction + wing_stretch, contenido alineado): {working_width}x{working_height}")
    for key, (scale, _, _, offset_x, offset_y) in normalization_plan.items():
        print(f"  {key}: content_scale={scale:.4f} offset=({offset_x},{offset_y})")

    canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(working_width, working_height)
    print(
        f"canvas lógico derivado: {canvas_width}x{canvas_height} (canvas de trabajo {working_width}x{working_height}, "
        f"referencia {prep_dev_sprite.REFERENCE_LOGICAL_SIZE} x DISPLAY_SIZE_SCALE_FACTOR={prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR})"
    )
    print(f"visual_scale (Block 05, por-pet, runtime): {VISUAL_SCALE} -> tamaño efectivo en pantalla "
          f"{round(canvas_width * VISUAL_SCALE)}x{round(canvas_height * VISUAL_SCALE)}")
    print(f"resolución de runtime (compilada): máximo {RUNTIME_MAX_FRAME_DIMENSION}px por lado, fuente sin tocar")

    right_dir = os.path.join("animations", "idle", "right", "frames")
    left_dir = os.path.join("animations", "idle", "left", "frames")
    right_frame_entries = [_frame_entry(right_dir, i) for i in range(right_report.frame_count)]
    left_frame_entries = [_frame_entry(left_dir, i) for i in range(left_report.frame_count)]

    click_right_dir = os.path.join("animations", "click_reaction", "right", "frames")
    click_left_dir = os.path.join("animations", "click_reaction", "left", "frames")
    click_right_frame_entries = [_frame_entry(click_right_dir, i) for i in range(click_right_report.frame_count)]
    click_left_frame_entries = [_frame_entry(click_left_dir, i) for i in range(click_left_report.frame_count)]

    wing_stretch_right_dir = os.path.join("animations", "wing_stretch", "right", "frames")
    wing_stretch_left_dir = os.path.join("animations", "wing_stretch", "left", "frames")
    wing_stretch_right_frame_entries = [_frame_entry(wing_stretch_right_dir, i) for i in range(wing_stretch_right_report.frame_count)]
    wing_stretch_left_frame_entries = [_frame_entry(wing_stretch_left_dir, i) for i in range(wing_stretch_left_report.frame_count)]

    idle_playback_fps = right_report.frame_count / EXPORT_DURATION_SECONDS
    click_playback_fps = click_right_report.frame_count / CLICK_EXPORT_DURATION_SECONDS
    wing_stretch_playback_fps = wing_stretch_right_report.frame_count / WING_STRETCH_EXPORT_DURATION_SECONDS
    print(f"click_reaction: {click_right_report.frame_count} frames reales @ {click_right_report.width}x{click_right_report.height}, fps derivado={click_playback_fps:.4f}")
    print(f"wing_stretch: {wing_stretch_right_report.frame_count} frames reales @ {wing_stretch_right_report.width}x{wing_stretch_right_report.height}, fps derivado={wing_stretch_playback_fps:.4f}")

    manifest = {
        "id": "nidir",
        "display_name": "Nidir",
        "variant_group": "",
        "canvas_width": canvas_width,
        "canvas_height": canvas_height,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "visual_scale": VISUAL_SCALE,
        "content_version": "block05-nidir-1",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
        "normalize_visual_scale": True,
        "states": [
            {
                "id": "default",
                "base_animation": {
                    "id": "idle_base",
                    "kind": "static",
                    "fps": 0,
                    "returns_to_idle": True,
                    "frames": [_frame_entry(right_dir, 0)],
                },
                "base_animation_direction_overrides": [
                    {
                        "direction": "left",
                        "id": "idle_base_left",
                        "kind": "static",
                        "fps": 0,
                        "returns_to_idle": True,
                        "frames": [_frame_entry(left_dir, 0)],
                    }
                ],
                "ambient_interval_seconds": AMBIENT_INTERVAL_SECONDS,
                "ambient_actions": [
                    {
                        "id": "idle_breathing",
                        "weight": AMBIENT_ACTION_WEIGHTS[0],
                        "target_state_id": "default",
                        "kind": "one_shot",
                        "fps": idle_playback_fps,
                        "returns_to_idle": True,
                        "frames": right_frame_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "idle_breathing_left",
                                "kind": "one_shot",
                                "fps": idle_playback_fps,
                                "returns_to_idle": True,
                                "frames": left_frame_entries,
                            }
                        ],
                    },
                    {
                        "id": "wing_stretch",
                        "weight": AMBIENT_ACTION_WEIGHTS[1],
                        "target_state_id": "default",
                        "kind": "one_shot",
                        "fps": wing_stretch_playback_fps,
                        "returns_to_idle": True,
                        "frames": wing_stretch_right_frame_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "wing_stretch_left",
                                "kind": "one_shot",
                                "fps": wing_stretch_playback_fps,
                                "returns_to_idle": True,
                                "frames": wing_stretch_left_frame_entries,
                            }
                        ],
                    },
                ],
                "hover_uses_ambient_actions": True,
                "hover_actions": [],
                "click_actions": [
                    {
                        "id": "click_fire",
                        "weight": 1.0,
                        "target_state_id": "default",
                        "kind": "one_shot",
                        "fps": click_playback_fps,
                        "returns_to_idle": True,
                        "frames": click_right_frame_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "click_fire_left",
                                "kind": "one_shot",
                                "fps": click_playback_fps,
                                "returns_to_idle": True,
                                "frames": click_left_frame_entries,
                            }
                        ],
                    }
                ],
            }
        ],
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote manifest: {MANIFEST_PATH}")

    pet_id, total_bytes = compile_pet_pack.compile_pack(MANIFEST_PATH, COMPILED_PATH)
    print(f"compiled pet '{pet_id}': {COMPILED_PATH} ({total_bytes} bytes)")

    # master.png := copia real de frame_000 de la pose base canónica
    # (Block 05, corrección post-QA -- ver
    # prep_dev_sprite.write_master_from_canonical_frame()). Nidir es
    # canónicamente "right".
    master_path = os.path.join(NIDIR_ROOT, "master.png")
    prep_dev_sprite.write_master_from_canonical_frame(master_path, os.path.join(RIGHT_FRAMES_DIR, "frame_000.png"))
    print(f"wrote master: {master_path} (copia de {RIGHT_FRAMES_DIR}/frame_000.png)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
