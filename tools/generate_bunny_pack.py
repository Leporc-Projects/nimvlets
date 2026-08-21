#!/usr/bin/env python3
"""Genera el contenido "right" de Bunny a partir de sus frames "left"
reales, ya importados en este repo (ver docs/BUNNY_CONTENT.md), y
compila el pack de runtime completo con el grafo de comportamiento de
un solo estado ("default") que todo pet normal usa (Block 05 -- ver
docs/ANIMATION_RUNTIME.md): pose base estática + click self-loop +
ambient self-loop ponderado 70/30 (grooming incluido) + hover
compartiendo el mismo pool ambient -- política de canvas de trabajo
compartido anclado por contenido sin cambios desde Block 04.3.

Bunny es canónicamente "left" (a diferencia de Nidir, "right") -- el
export real nombra su propia dirección canónica así (ver
assets/source/nimvlets/bunny/DESCRIPTION.txt). Este script deriva
"right" por espejado horizontal determinista de los frames "left"
reales, y wirea el campo CANÓNICO del manifest (resuelto para
Direction::kRight por content::ResolveAnimation()) con los frames
DERIVADOS, y el override "left" con los frames REALES -- inverso de
dónde vive cada uno en disco, pero lo que el runtime necesita para que
kRight/kLeft resuelvan al contenido visualmente correcto.

Block 05 (corrección de comportamiento + escala visual, ver el informe
de este bloque):
    - `ambient_interval_seconds` pasa de 10.0 (Block 04.3) a 15.0 --
      política de producto vigente ("target interval is now 15
      seconds"), un único valor para Bunny y Nidir.
    - `visual_scale` nuevo (por-pet, runtime -- ver
      content::PetDefinition::visualScale): Bunny queda en el default
      1.0 ("Bunny's current size is approved", sin cambio) -- ver
      VISUAL_SCALE abajo y tools/generate_nidir_pack.py para el
      contraste con Nidir (1.10).
    - Diagnóstico raíz de "pixel/body-part loss during playback":
      NINGUNA etapa de este pipeline (fuente -> compose -> compilación
      -> render) demostró pérdida real de contenido medible -- ver el
      informe de este bloque. El hallazgo de "clipping real" de
      docs/BUNNY_CONTENT.md §3.1 queda CORREGIDO/retractado con
      evidencia de gradiente de alpha real (un borde antialiaseado
      normal de 2-3px, no un corte plano). Este script no cambia por
      eso -- no había ningún bug de compilación que corregir acá.

Uso:
    python3 tools/generate_bunny_pack.py

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
BUNNY_ROOT = os.path.join(REPO_ROOT, "assets", "source", "nimvlets", "bunny")

IDLE_ROOT = os.path.join(BUNNY_ROOT, "animations", "idle")
IDLE_CANONICAL_FRAMES_DIR = os.path.join(IDLE_ROOT, "left", "frames")
IDLE_DERIVED_FRAMES_DIR = os.path.join(IDLE_ROOT, "right", "frames")
IDLE_CANONICAL_SPRITESHEET_PATH = os.path.join(IDLE_ROOT, "left", "spritesheet", "spritesheet.png")
IDLE_DERIVED_SPRITESHEET_PATH = os.path.join(IDLE_ROOT, "right", "spritesheet", "spritesheet.png")

CLICK_ROOT = os.path.join(BUNNY_ROOT, "animations", "click_reaction")
CLICK_CANONICAL_FRAMES_DIR = os.path.join(CLICK_ROOT, "left", "frames")
CLICK_DERIVED_FRAMES_DIR = os.path.join(CLICK_ROOT, "right", "frames")
CLICK_CANONICAL_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "left", "spritesheet", "spritesheet.png")
CLICK_DERIVED_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "right", "spritesheet", "spritesheet.png")

GROOM_ROOT = os.path.join(BUNNY_ROOT, "animations", "groom")
GROOM_CANONICAL_FRAMES_DIR = os.path.join(GROOM_ROOT, "left", "frames")
GROOM_DERIVED_FRAMES_DIR = os.path.join(GROOM_ROOT, "right", "frames")
GROOM_CANONICAL_SPRITESHEET_PATH = os.path.join(GROOM_ROOT, "left", "spritesheet", "spritesheet.png")
GROOM_DERIVED_SPRITESHEET_PATH = os.path.join(GROOM_ROOT, "right", "spritesheet", "spritesheet.png")

MANIFEST_PATH = os.path.join(BUNNY_ROOT, "pack_manifest.json")
COMPILED_PATH = os.path.join(REPO_ROOT, "assets", "dev", "bunny_pack.nvpack")

RUNTIME_MAX_FRAME_DIMENSION = 2 * prep_dev_sprite.REFERENCE_LOGICAL_SIZE

GRID_COLUMNS = 5

ALPHA_HIT_THRESHOLD = 128

EXPORT_DURATION_SECONDS = 3.0

# 15 segundos -- política de producto vigente para Block 05 ("target
# interval is now 15 seconds"), reemplaza el 10.0 de la corrección
# post-QA de Block 04.3. Mismo valor que Nidir -- un único intervalo de
# producto para los dos, no un ajuste por-pet.
AMBIENT_INTERVAL_SECONDS = 15.0

# Selección ponderada 70/30 entre las DOS acciones ambient de Bunny --
# índice 0 = idle periódico (breathing), índice 1 = groom.
AMBIENT_ACTION_WEIGHTS = [0.7, 0.3]

# Escala visual por-pet (Block 05 -- ver content::PetDefinition::
# visualScale). "Bunny's current size is approved" -- 1.0 preserva
# exactamente el tamaño en pantalla que ya tenía (134x176, derivado del
# canvas de trabajo compartido de abajo con el factor de referencia
# global existente, sin cambios). Contraste: tools/generate_nidir_pack.py
# usa 1.10 -- ver el informe de este bloque para el resultado exacto de
# cada uno.
VISUAL_SCALE = 1.0


def _assemble_spritesheet_from_frames(frame_dir: str, frame_count: int, frame_w: int, frame_h: int) -> tuple[int, int, bytes]:
    """Ensambla un spritesheet 5xN (row-major) a partir de frames ya
    normalizados en disco -- nunca espeja/transforma el spritesheet
    como imagen completa."""
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


def _derive_mirrored_direction(
    label: str,
    canonical_frames_dir: str,
    derived_frames_dir: str,
    canonical_spritesheet_path: str,
    derived_spritesheet_path: str,
) -> tuple[validate_frame_sequence.FrameSequenceReport, validate_frame_sequence.FrameSequenceReport]:
    canonical_report = validate_frame_sequence.validate_frame_sequence(canonical_frames_dir, ALPHA_HIT_THRESHOLD)
    print(f"{label}: frames canónicos válidos: {canonical_report.frame_count} @ {canonical_report.width}x{canonical_report.height}")

    if not os.path.isfile(canonical_spritesheet_path):
        raise SystemExit(f"error: falta el spritesheet canónico importado de {label}: {canonical_spritesheet_path}")

    os.makedirs(derived_frames_dir, exist_ok=True)
    os.makedirs(os.path.dirname(derived_spritesheet_path), exist_ok=True)

    for index in range(canonical_report.frame_count):
        src_path = os.path.join(canonical_frames_dir, f"frame_{index:03d}.png")
        w, h, pixels = prep_dev_sprite.read_png_rgba(src_path)
        mirrored = prep_dev_sprite.mirror_rgba_horizontal(w, h, pixels)
        dst_path = os.path.join(derived_frames_dir, f"frame_{index:03d}.png")
        prep_dev_sprite.write_png_rgba(dst_path, w, h, mirrored)

    derived_report = validate_frame_sequence.validate_frame_sequence(derived_frames_dir, ALPHA_HIT_THRESHOLD)
    print(f"{label}: frames derivados generados y validados: {derived_report.frame_count} @ {derived_report.width}x{derived_report.height}")
    assert derived_report.frame_count == canonical_report.frame_count
    assert (derived_report.width, derived_report.height) == (canonical_report.width, canonical_report.height)
    assert derived_report.transparent_fraction_range == canonical_report.transparent_fraction_range, (
        f"{label}: el espejado horizontal no debería alterar la fracción de pixeles transparentes de ningún frame"
    )

    sheet_w, sheet_h, sheet_pixels = _assemble_spritesheet_from_frames(
        derived_frames_dir, derived_report.frame_count, derived_report.width, derived_report.height
    )
    prep_dev_sprite.write_png_rgba(derived_spritesheet_path, sheet_w, sheet_h, sheet_pixels)
    print(f"{label}: spritesheet derivado ensamblado: {derived_spritesheet_path} ({sheet_w}x{sheet_h})")

    return canonical_report, derived_report


def _frame_entry(rel_dir: str, index: int) -> dict:
    return {"source": os.path.join(rel_dir, f"frame_{index:03d}.png"), "duration_ms": 0}


def main() -> int:
    idle_left_report, idle_right_report = _derive_mirrored_direction(
        "idle", IDLE_CANONICAL_FRAMES_DIR, IDLE_DERIVED_FRAMES_DIR, IDLE_CANONICAL_SPRITESHEET_PATH, IDLE_DERIVED_SPRITESHEET_PATH
    )
    click_left_report, click_right_report = _derive_mirrored_direction(
        "click_reaction", CLICK_CANONICAL_FRAMES_DIR, CLICK_DERIVED_FRAMES_DIR, CLICK_CANONICAL_SPRITESHEET_PATH, CLICK_DERIVED_SPRITESHEET_PATH
    )
    groom_left_report, groom_right_report = _derive_mirrored_direction(
        "groom", GROOM_CANONICAL_FRAMES_DIR, GROOM_DERIVED_FRAMES_DIR, GROOM_CANONICAL_SPRITESHEET_PATH, GROOM_DERIVED_SPRITESHEET_PATH
    )

    normalization_entries = {
        "idle": prep_dev_sprite.read_png_rgba(os.path.join(IDLE_CANONICAL_FRAMES_DIR, "frame_000.png")),
        "idle_right": prep_dev_sprite.read_png_rgba(os.path.join(IDLE_DERIVED_FRAMES_DIR, "frame_000.png")),
        "click_reaction": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_CANONICAL_FRAMES_DIR, "frame_000.png")),
        "click_reaction_right": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_DERIVED_FRAMES_DIR, "frame_000.png")),
        "groom": prep_dev_sprite.read_png_rgba(os.path.join(GROOM_CANONICAL_FRAMES_DIR, "frame_000.png")),
        "groom_right": prep_dev_sprite.read_png_rgba(os.path.join(GROOM_DERIVED_FRAMES_DIR, "frame_000.png")),
    }
    normalization_groups = {
        "idle": "idle",
        "idle_right": "idle",
        "click_reaction": "click_reaction",
        "click_reaction_right": "click_reaction",
        "groom": "groom",
        "groom_right": "groom",
    }
    # Bunny tiene un solo BehaviorState ("default") -- state_of_group/
    # base_group_of_state son triviales acá (todo bajo el mismo
    # estado), y group_frame_paths usa las rutas REALES de frame_000
    # de cada grupo (idle/click_reaction/groom nunca comparten archivo
    # entre sí en Bunny, así que el union-find de
    # compute_frame_normalization_plan no fusiona nada -- mismo
    # resultado que antes de Block 05, segunda pasada. Ver esa función
    # y docs/DECISION_LOG.md DEC-075 para el mecanismo completo, que
    # solo importa de verdad para un pet con 2+ estados como Frin).
    normalization_state_of_group = {"idle": "default", "click_reaction": "default", "groom": "default"}
    normalization_base_group_of_state = {"default": "idle"}
    normalization_group_frame_paths = {
        "idle": [os.path.realpath(os.path.join(IDLE_CANONICAL_FRAMES_DIR, "frame_000.png"))],
        "click_reaction": [os.path.realpath(os.path.join(CLICK_CANONICAL_FRAMES_DIR, "frame_000.png"))],
        "groom": [os.path.realpath(os.path.join(GROOM_CANONICAL_FRAMES_DIR, "frame_000.png"))],
    }
    normalization_plan = prep_dev_sprite.compute_frame_normalization_plan(
        normalization_entries, normalization_groups, reference_group="idle",
        group_frame_paths=normalization_group_frame_paths, state_of_group=normalization_state_of_group,
        base_group_of_state=normalization_base_group_of_state,
    )
    _, working_width, working_height, _, _ = normalization_plan["idle"]
    print(f"canvas de trabajo compartido (idle + click_reaction + groom, contenido alineado): {working_width}x{working_height}")
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

    left_dir = os.path.join("animations", "idle", "left", "frames")
    right_dir = os.path.join("animations", "idle", "right", "frames")
    idle_left_entries = [_frame_entry(left_dir, i) for i in range(idle_left_report.frame_count)]
    idle_right_entries = [_frame_entry(right_dir, i) for i in range(idle_right_report.frame_count)]

    click_left_dir = os.path.join("animations", "click_reaction", "left", "frames")
    click_right_dir = os.path.join("animations", "click_reaction", "right", "frames")
    click_left_entries = [_frame_entry(click_left_dir, i) for i in range(click_left_report.frame_count)]
    click_right_entries = [_frame_entry(click_right_dir, i) for i in range(click_right_report.frame_count)]

    groom_left_dir = os.path.join("animations", "groom", "left", "frames")
    groom_right_dir = os.path.join("animations", "groom", "right", "frames")
    groom_left_entries = [_frame_entry(groom_left_dir, i) for i in range(groom_left_report.frame_count)]
    groom_right_entries = [_frame_entry(groom_right_dir, i) for i in range(groom_right_report.frame_count)]

    idle_playback_fps = idle_left_report.frame_count / EXPORT_DURATION_SECONDS
    click_playback_fps = click_left_report.frame_count / EXPORT_DURATION_SECONDS
    groom_playback_fps = groom_left_report.frame_count / EXPORT_DURATION_SECONDS
    print(f"idle: {idle_left_report.frame_count} frames reales, fps derivado={idle_playback_fps:.4f}")
    print(f"click_reaction: {click_left_report.frame_count} frames reales, fps derivado={click_playback_fps:.4f}")
    print(f"groom: {groom_left_report.frame_count} frames reales, fps derivado={groom_playback_fps:.4f}")

    # IMPORTANTE -- Bunny es canónicamente "left" (a diferencia de
    # Nidir): el campo canónico (kRight) de cada animación usa los
    # frames DERIVADOS (espejados, "right"); el override "left" usa los
    # frames REALES importados -- ver el docstring del módulo.
    manifest = {
        "id": "bunny",
        "display_name": "Bunny",
        "variant_group": "",
        "canvas_width": canvas_width,
        "canvas_height": canvas_height,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "visual_scale": VISUAL_SCALE,
        "content_version": "block05-bunny-1",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
        "normalize_visual_scale": True,
        "states": [
            {
                "id": "default",
                "base_animation": {
                    "id": "idle_base_right",
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
                        "frames": idle_right_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "idle_breathing_left",
                                "kind": "one_shot",
                                "fps": idle_playback_fps,
                                "returns_to_idle": True,
                                "frames": idle_left_entries,
                            }
                        ],
                    },
                    {
                        "id": "groom",
                        "weight": AMBIENT_ACTION_WEIGHTS[1],
                        "target_state_id": "default",
                        "kind": "one_shot",
                        "fps": groom_playback_fps,
                        "returns_to_idle": True,
                        "frames": groom_right_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "groom_left",
                                "kind": "one_shot",
                                "fps": groom_playback_fps,
                                "returns_to_idle": True,
                                "frames": groom_left_entries,
                            }
                        ],
                    },
                ],
                # "hover uses the same available passive-action pool
                # unless content says otherwise" -- Bunny no dice
                # otherwise, así que hover comparte el pool ambient de
                # arriba (sin duplicar frames en el pack compilado).
                "hover_uses_ambient_actions": True,
                "hover_actions": [],
                "click_actions": [
                    {
                        "id": "click",
                        "weight": 1.0,
                        "target_state_id": "default",
                        "kind": "one_shot",
                        "fps": click_playback_fps,
                        "returns_to_idle": True,
                        "frames": click_right_entries,
                        "direction_overrides": [
                            {
                                "direction": "left",
                                "id": "click_left",
                                "kind": "one_shot",
                                "fps": click_playback_fps,
                                "returns_to_idle": True,
                                "frames": click_left_entries,
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
    # prep_dev_sprite.write_master_from_canonical_frame()). Bunny es
    # canónicamente "left" -- master.png usa el frame REAL importado,
    # nunca el derivado por espejo.
    master_path = os.path.join(BUNNY_ROOT, "master.png")
    prep_dev_sprite.write_master_from_canonical_frame(master_path, os.path.join(IDLE_CANONICAL_FRAMES_DIR, "frame_000.png"))
    print(f"wrote master: {master_path} (copia de {IDLE_CANONICAL_FRAMES_DIR}/frame_000.png)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
