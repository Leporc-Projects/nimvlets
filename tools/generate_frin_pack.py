#!/usr/bin/env python3
"""Genera los DOS packs de runtime de Frin (macho y hembra) a partir de
sus frames reales ya importados en este repo (ver
assets/source/nimvlets/frin/{male,female}/ y el informe de Block 05).

Frin es UN Nimvlet lógico con DOS variantes visuales -- mismo grafo de
comportamiento con nombres de estado idénticos ("seated"/"lying") para
ambas, cada una con su propio contenido real. Este script construye
ambos manifests con la MISMA función (`_build_variant_manifest()`), así
que "comparten el modelo de comportamiento" es una garantía de código,
no solo una convención documentada -- nunca hay dos copias de la lógica
de armado que puedan divergir por accidente.

Grafo de comportamiento (dos BehaviorState, ver
docs/ANIMATION_RUNTIME.md y content::AnimationController):

    seated (estado inicial):
      base_animation: frame_000 de sit_to_lie (la pose sentada real, sin
        inventar un frame nuevo -- "starts seated using the master/base
        pose").
      ambient: sit_to_lie (one-shot) -> lying, tras un rest-delay
        autorable por contenido (REST_DELAY_SECONDS, no hardcodeado por
        especie).
      click (ponderado 70/30): howl -> seated / tail_greet -> seated.
      sin hover propio -- el owner no definió ninguno todavía.

    lying:
      base_animation: frame_024 (el último) de sit_to_lie -- "the lying
        pose can be represented by the proper final frame/state" -- se
        REFERENCIA el mismo archivo ya importado, nunca se duplica ni
        se inventa un asset nuevo.
      sin ambient (nunca hay howl/tail-greet aleatorio mientras lying).
      click: lie_to_sit (one-shot) -> seated.
      sin hover propio.

Direcciones canónicas, de los nombres de export FINALES del owner:
    macho: LEFT (frin-macho-*-left)
    hembra: RIGHT (frin-hembra-*-right)
Mismo cuidado de inversión que ya usa tools/generate_bunny_pack.py
(canónico "left"): el campo CANÓNICO de cada animación (resuelto para
Direction::kRight) usa los frames DERIVADOS (espejados) para el macho,
y los frames REALES para la hembra -- y viceversa para el override
"left" -- ver _build_variant_manifest().

Uso:
    python3 tools/generate_frin_pack.py

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
FRIN_ROOT = os.path.join(REPO_ROOT, "assets", "source", "nimvlets", "frin")

ANIMATION_IDS = ("sit_to_lie", "lie_to_sit", "howl", "tail_greet")

GRID_COLUMNS = 5
ALPHA_HIT_THRESHOLD = 128
EXPORT_DURATION_SECONDS = 3.0
RUNTIME_MAX_FRAME_DIMENSION = 2 * prep_dev_sprite.REFERENCE_LOGICAL_SIZE

# Unificado con el intervalo ambient base de Bunny/Nidir (ver
# tools/generate_bunny_pack.py) -- 45.0 (DEC-066, nunca confirmado por
# el owner) -> 15.0 (DEC-074, "cambia el tiempo base de las animaciones
# pasivas a 15 segundos por ahora") -> 12.0 (pasada de resolución de
# renderer, pedido de producto explícito, ver DEC-084 en
# docs/DECISION_LOG.md). Dato de CONTENIDO (por-estado, ver
# BehaviorState::ambientIntervalSeconds), nunca hardcodeado por
# especie -- Artu (futuro, misma forma de grafo) puede definir su
# propio valor sin tocar ningún código.
REST_DELAY_SECONDS = 12.0

# 70/30 entre howl y tail_greet -- mismo mecanismo genérico que el
# ambient 70/30 de Bunny/Nidir (content::ChooseWeightedActionIndex()),
# acá aplicado al trigger de click en vez de ambient.
SEATED_CLICK_WEIGHTS = {"howl": 0.7, "tail_greet": 0.3}

# Escala visual por-pet (Block 05, segunda pasada de corrección
# post-QA -- ver docs/DECISION_LOG.md DEC-076). QA manual real: "Frin
# male/female currently feel too small; visibly larger, comparable in
# presence to Bunny/Nidir." Medido con el bounding box de contenido
# visible del pack compilado (no solo el canvas transparente): a
# 1.0, Frin macho ~85x128pt / hembra ~91x138pt -- notablemente más
# chico que Bunny (~114x159pt, la referencia aprobada). A 1.30:
# macho ~110x166pt / hembra ~118x179pt -- comparable a Bunny/Nidir
# (~154x176pt) en presencia de escritorio. UN SOLO valor para ambas
# variantes (mismo personaje, mismo tamaño percibido esperado, sin
# importar el género) -- trivial de ajustar: cambiar este único
# número y volver a correr este script.
VISUAL_SCALE = 1.30


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


def _derive_mirrored_direction(label: str, canonical_dir: str, derived_dir: str, canonical_sheet: str, derived_sheet: str):
    report = validate_frame_sequence.validate_frame_sequence(canonical_dir, ALPHA_HIT_THRESHOLD)
    print(f"{label}: frames reales válidos: {report.frame_count} @ {report.width}x{report.height}")
    if not os.path.isfile(canonical_sheet):
        raise SystemExit(f"error: falta el spritesheet real importado de {label}: {canonical_sheet}")

    os.makedirs(derived_dir, exist_ok=True)
    os.makedirs(os.path.dirname(derived_sheet), exist_ok=True)
    for index in range(report.frame_count):
        w, h, pixels = prep_dev_sprite.read_png_rgba(os.path.join(canonical_dir, f"frame_{index:03d}.png"))
        mirrored = prep_dev_sprite.mirror_rgba_horizontal(w, h, pixels)
        prep_dev_sprite.write_png_rgba(os.path.join(derived_dir, f"frame_{index:03d}.png"), w, h, mirrored)

    derived_report = validate_frame_sequence.validate_frame_sequence(derived_dir, ALPHA_HIT_THRESHOLD)
    assert derived_report.frame_count == report.frame_count
    assert (derived_report.width, derived_report.height) == (report.width, report.height)
    assert derived_report.transparent_fraction_range == report.transparent_fraction_range

    sheet_w, sheet_h, sheet_pixels = _assemble_spritesheet_from_frames(derived_dir, derived_report.frame_count, derived_report.width, derived_report.height)
    prep_dev_sprite.write_png_rgba(derived_sheet, sheet_w, sheet_h, sheet_pixels)
    print(f"{label}: frames derivados generados y validados: {derived_report.frame_count} @ {derived_report.width}x{derived_report.height}")

    return report, derived_report


def _frame_entry(rel_dir: str, index: int) -> dict:
    return {"source": os.path.join(rel_dir, f"frame_{index:03d}.png"), "duration_ms": 0}


def _build_variant_manifest(variant: str, pet_id: str, display_name: str, canonical_direction: str) -> tuple[dict, dict]:
    """Construye el manifest de UN variante de Frin. `canonical_direction`
    es "left" (macho) o "right" (hembra) -- la dirección REAL importada
    para este variante; la opuesta se deriva acá por espejado
    determinista. Retorna (manifest, reports) -- reports solo para
    logging en main()."""
    variant_root = os.path.join(FRIN_ROOT, variant)
    opposite_direction = "right" if canonical_direction == "left" else "left"

    reports: dict[str, validate_frame_sequence.FrameSequenceReport] = {}
    frame_entries: dict[str, list[dict]] = {}
    fps_by_anim: dict[str, float] = {}

    for anim_id in ANIMATION_IDS:
        anim_root = os.path.join(variant_root, "animations", anim_id)
        real_dir = os.path.join(anim_root, canonical_direction, "frames")
        derived_dir = os.path.join(anim_root, opposite_direction, "frames")
        real_sheet = os.path.join(anim_root, canonical_direction, "spritesheet", "spritesheet.png")
        derived_sheet = os.path.join(anim_root, opposite_direction, "spritesheet", "spritesheet.png")

        real_report, derived_report = _derive_mirrored_direction(f"{variant}/{anim_id}", real_dir, derived_dir, real_sheet, derived_sheet)
        reports[anim_id] = real_report
        fps_by_anim[anim_id] = real_report.frame_count / EXPORT_DURATION_SECONDS

        real_rel_dir = os.path.join("animations", anim_id, canonical_direction, "frames")
        derived_rel_dir = os.path.join("animations", anim_id, opposite_direction, "frames")
        frame_entries[f"{anim_id}_real"] = [_frame_entry(real_rel_dir, i) for i in range(real_report.frame_count)]
        frame_entries[f"{anim_id}_derived"] = [_frame_entry(derived_rel_dir, i) for i in range(derived_report.frame_count)]

    # canonical (kRight) siempre usa los frames "right"; el override
    # kLeft siempre usa los frames "left" -- cuál de los dos ("real"
    # importado o "derivado" por espejo) vive en cada uno depende de
    # canonical_direction, exactamente el mismo cuidado de inversión que
    # tools/generate_bunny_pack.py ya documenta.
    def entries_for(anim_id: str, direction: str) -> list[dict]:
        return frame_entries[f"{anim_id}_real"] if direction == canonical_direction else frame_entries[f"{anim_id}_derived"]

    def action(anim_id: str, weight: float, target_state_id: str) -> dict:
        return {
            "id": anim_id,
            "weight": weight,
            "target_state_id": target_state_id,
            "kind": "one_shot",
            "fps": fps_by_anim[anim_id],
            "returns_to_idle": True,
            "frames": entries_for(anim_id, "right"),
            "direction_overrides": [
                {
                    "direction": "left",
                    "id": f"{anim_id}_left",
                    "kind": "one_shot",
                    "fps": fps_by_anim[anim_id],
                    "returns_to_idle": True,
                    "frames": entries_for(anim_id, "left"),
                }
            ],
        }

    # Poses base: referencian frames YA importados de sit_to_lie (frame
    # 0 = sentado, frame final = acostado) -- nunca se duplica ni se
    # inventa un asset nuevo ("do not invent another art asset
    # unnecessarily").
    seated_frame_index = 0
    lying_frame_index = reports["sit_to_lie"].frame_count - 1

    def base_pose(direction: str, frame_index: int, pose_id: str) -> dict:
        return {
            "id": pose_id,
            "kind": "static",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [entries_for("sit_to_lie", direction)[frame_index]],
        }

    manifest = {
        "id": pet_id,
        "display_name": display_name,
        "variant_group": "frin",
        # canvas_width/height se completan más abajo, tras derivar el
        # plan de normalización (igual que Bunny/Nidir).
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "visual_scale": VISUAL_SCALE,
        "content_version": "block05-frin-1",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
        "normalize_visual_scale": True,
        "states": [
            {
                "id": "seated",
                "base_animation": base_pose("right", seated_frame_index, "seated_base"),
                "base_animation_direction_overrides": [
                    {"direction": "left", **base_pose("left", seated_frame_index, "seated_base_left")}
                ],
                "ambient_interval_seconds": REST_DELAY_SECONDS,
                "ambient_actions": [action("sit_to_lie", 1.0, "lying")],
                "hover_uses_ambient_actions": False,  # el owner no definió hover para Frin todavía
                "hover_actions": [],
                "click_actions": [
                    action("howl", SEATED_CLICK_WEIGHTS["howl"], "seated"),
                    action("tail_greet", SEATED_CLICK_WEIGHTS["tail_greet"], "seated"),
                ],
            },
            {
                "id": "lying",
                "base_animation": base_pose("right", lying_frame_index, "lying_base"),
                "base_animation_direction_overrides": [
                    {"direction": "left", **base_pose("left", lying_frame_index, "lying_base_left")}
                ],
                "ambient_interval_seconds": 0.0,  # sin efecto -- ambient_actions vacío
                "ambient_actions": [],  # "No random howl/tail-greet while lying"
                "hover_uses_ambient_actions": False,
                "hover_actions": [],
                "click_actions": [action("lie_to_sit", 1.0, "seated")],
            },
        ],
    }
    return manifest, reports


def _finalize_and_compile(variant: str, manifest: dict, canonical_direction: str) -> None:
    variant_root = os.path.join(FRIN_ROOT, variant)
    manifest_path = os.path.join(variant_root, "pack_manifest.json")

    normalization_plan = compile_pet_pack._build_normalization_plan(manifest, variant_root)
    _, working_width, working_height, _, _ = normalization_plan[f"state[{manifest['states'][0]['id']}].base_animation"]
    canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(working_width, working_height)
    manifest["canvas_width"] = canvas_width
    manifest["canvas_height"] = canvas_height
    print(
        f"{variant}: canvas de trabajo compartido (todas las animaciones, contenido alineado): "
        f"{working_width}x{working_height} -> canvas lógico {canvas_width}x{canvas_height} "
        f"(visual_scale={manifest['visual_scale']} -> efectivo "
        f"{round(canvas_width * manifest['visual_scale'])}x{round(canvas_height * manifest['visual_scale'])})"
    )

    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote manifest: {manifest_path}")

    compiled_path = os.path.join(REPO_ROOT, "assets", "dev", f"frin_{variant}_pack.nvpack")
    pet_id, total_bytes = compile_pet_pack.compile_pack(manifest_path, compiled_path)
    print(f"compiled pet '{pet_id}': {compiled_path} ({total_bytes} bytes)")

    # master.png := copia real de frame_000 de la pose base canónica
    # (Block 05, corrección post-QA -- ver
    # prep_dev_sprite.write_master_from_canonical_frame()). La pose
    # base de "seated" (el estado inicial) es sit_to_lie frame 0 en la
    # dirección canónica de este variante -- mismo frame que
    # base_pose() ya referencia en _build_variant_manifest(), nunca un
    # asset separado.
    canonical_frame_path = os.path.join(
        variant_root, "animations", "sit_to_lie", canonical_direction, "frames", "frame_000.png"
    )
    master_path = os.path.join(variant_root, "master.png")
    prep_dev_sprite.write_master_from_canonical_frame(master_path, canonical_frame_path)
    print(f"wrote master: {master_path} (copia de {canonical_frame_path})")


def main() -> int:
    male_manifest, male_reports = _build_variant_manifest("male", "frin_male", "Frin", "left")
    print(f"macho: sit_to_lie={male_reports['sit_to_lie'].frame_count}f lie_to_sit={male_reports['lie_to_sit'].frame_count}f "
          f"howl={male_reports['howl'].frame_count}f tail_greet={male_reports['tail_greet'].frame_count}f")
    _finalize_and_compile("male", male_manifest, "left")

    female_manifest, female_reports = _build_variant_manifest("female", "frin_female", "Frin", "right")
    print(f"hembra: sit_to_lie={female_reports['sit_to_lie'].frame_count}f lie_to_sit={female_reports['lie_to_sit'].frame_count}f "
          f"howl={female_reports['howl'].frame_count}f tail_greet={female_reports['tail_greet'].frame_count}f")
    _finalize_and_compile("female", female_manifest, "right")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
