#!/usr/bin/env python3
"""Genera el contenido "right" de Bunny a partir de sus frames "left"
reales, ya importados en este repo (ver docs/BUNNY_CONTENT.md), y
compila el pack de runtime completo con la MISMA semántica de
producto ya establecida y corregida para Nidir en Block 04.2/04.3:
pose base estática + idle periódico esporádico (one-shot) + click
(one-shot) + política de canvas de trabajo compartido anclado por
contenido — ver docs/DECISION_LOG.md.

Block 04.3 migra a Bunny de fixture de QA sintético (Block 01/02, ver
tools/generate_bunny_dev_pack.py -- SUPERSEDED, ver su docstring) a
contenido real de producción: el owner exportó su idle y click real
(`local_imports/bunny/bunny-idle-left/`,
`local_imports/bunny/bunny-click-left/`), ya copiados una única vez a
sus rutas canónicas en `assets/source/nimvlets/bunny/animations/`
(ver el commit que importó estos assets). Esta importación fue el
segundo caso REAL (después de Nidir) de este pipeline, sirviendo como
segunda validación genuina de la política de normalización visual/
canvas de trabajo compartido y de la extensión direccional del
content model -- ambas ya genéricas desde que se implementaron para
Nidir, sin ningún cambio necesario para que funcionen con un pet
distinto.

A diferencia de Nidir (cuyo export real trae "right" como dirección
canónica), el export real de Bunny nombra su propia dirección
canónica "left" -- adoptado tal cual (ver
assets/source/nimvlets/bunny/DESCRIPTION.txt, "dirección canónica").
Este script deriva "right" por el mismo espejado horizontal
determinista que ya usa tools/generate_nidir_pack.py, solo que en la
dirección opuesta -- la lógica en sí es genérica (una función que
recibe "cuál carpeta es la canónica importada" y "cuál hay que
derivar", sin asumir cuál lado es cuál).

Solo se integran las animaciones que HOY existen en el export real del
owner -- static/base master, un idle periódico, un click_reaction --
por instrucción explícita del brief de este bloque. NO se implementa
acá (deliberadamente diferido a un futuro bloque de interacción,
después de que exista el arte nuevo): una segunda animación de idle,
el comportamiento ponderado 70/30 entre dos idles, ni un disparador
por hover. El scheduler de acciones pasivas ya soporta una LISTA de
`passive_actions` (ver content::PetDefinition::passiveActions) -- una
segunda entrada real, cuando exista el arte, se agrega ahí sin ningún
cambio de arquitectura, exactamente el mismo patrón "generic, no
per-pet C++" que este bloque ya demostró para Nidir.

Escribe, para CADA animación (idle, click_reaction):
    assets/source/nimvlets/bunny/animations/<anim>/right/frames/frame_NNN.png
        (espejo horizontal exacto de cada frame "left")
    assets/source/nimvlets/bunny/animations/<anim>/right/spritesheet/spritesheet.png
        (ensamblado desde los frames "right" ya espejados, en la misma
        grilla 5x5 que el spritesheet "left" original -- nunca espejar
        la imagen del spritesheet completo de una sola vez)
Y además:
    assets/source/nimvlets/bunny/pack_manifest.json
    assets/dev/bunny_pack.nvpack
        (mismo path que el fixture sintético anterior -- ver
        "identidad de catálogo" más abajo)

Determinista: re-correr este script contra los mismos frames "left"
produce salida byte-idéntica. Este script NO toca `local_imports/` --
staging temporal del owner, nunca commiteado (ver .git/info/exclude);
el copiado desde ahí hacia `assets/source/nimvlets/bunny/animations/`
ya se hizo una única vez, de forma manual y documentada en el commit
que importó estos assets (mismo patrón que el import de Nidir: los
frames ya venían correctamente nombrados/normalizados, "zero actual
normalization" needed, así que un `cp` directo bastó).

Identidad de catálogo (migración cuidadosa, sin romper contratos):
`id` se mantiene como "bunny_dev" (NO se renombra a "bunny") y
`assets/dev/bunny_pack.nvpack` se mantiene como el mismo path exacto
que el fixture sintético anterior -- así, cualquier estado persistido
existente (`AppState.activePetId == "bunny_dev"`,
`assets/dev/pet_catalog_manifest.json`'s `pet_id`/`pack_path`) sigue
resolviendo correctamente sin ningún cambio, aunque el CONTENIDO real
del pack ya no sea sintético. `display_name` sí se actualiza (de
"Bunny (dev fixture)" a "Bunny") -- eso es solo una etiqueta legible,
no una clave de identidad, y ya no es honesto llamarlo "fixture" una
vez que el contenido es real.

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

MANIFEST_PATH = os.path.join(BUNNY_ROOT, "pack_manifest.json")
COMPILED_PATH = os.path.join(REPO_ROOT, "assets", "dev", "bunny_pack.nvpack")

# Mismo margen "2x densidad de pixel de referencia" que Nidir usa (ver
# tools/generate_nidir_pack.py) -- genérico, no un valor propio de
# Bunny.
RUNTIME_MAX_FRAME_DIMENSION = 2 * prep_dev_sprite.REFERENCE_LOGICAL_SIZE

# Grilla del spritesheet: 5 columnas -- confirmado inspeccionando las
# dimensiones reales de los dos spritesheets importados (idle:
# 1920x2685 == 5*384 x 5*537; click: 2095x2935 == 5*419 x 5*587,
# ambos exactos), no asumido.
GRID_COLUMNS = 5

# 128: mismo punto medio estándar de borde antialiaseado que Nidir/la
# convención original de Bunny (DEC-018) -- confirmado con el
# histograma real del frame_000 de idle de este export: fondo en
# alpha=0 (45.5%), interior en alpha>=250 (53.3%), solo 1.2% de
# pixeles en la banda de borde antialiaseado entre medio, donde el
# umbral realmente importa.
ALPHA_HIT_THRESHOLD = 128

# Misma configuración de Ludo.ai que Nidir usó ("3 seconds, Max Frames
# 25") -- el owner no proveyó un dato separado para este export de
# Bunny específicamente, pero SÍ trae 25 frames como Nidir, consistente
# con esa misma configuración. Suposición explícita y documentada, no
# confirmada por separado -- si se confirma un valor distinto en el
# futuro, ajustar acá.
EXPORT_DURATION_SECONDS = 3.0

# Sin cambios respecto al fixture sintético anterior de Bunny -- este
# bloque solo cambió el valor de Nidir (a 60s, pedido explícito del
# owner); nada pidió cambiar el de Bunny, así que se mantiene el
# default de PetDefinition.
PASSIVE_INTERVAL_SECONDS = 300.0


def _assemble_spritesheet_from_frames(frame_dir: str, frame_count: int, frame_w: int, frame_h: int) -> tuple[int, int, bytes]:
    """Ensambla un spritesheet 5xN (row-major) a partir de frames ya
    normalizados en disco -- nunca espeja/transforma el spritesheet
    como imagen completa (ver el docstring del módulo)."""
    rows = (frame_count + GRID_COLUMNS - 1) // GRID_COLUMNS
    sheet_w = GRID_COLUMNS * frame_w
    sheet_h = rows * frame_h
    sheet = bytearray(sheet_w * sheet_h * 4)  # transparente por defecto (todo-ceros == alpha 0)

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
    """Valida los frames de la dirección CANÓNICA reales ya importados
    de una animación, deriva la dirección OPUESTA por espejado
    horizontal determinista, la valida contra el mismo contrato, y
    ensambla su spritesheet desde los frames ya espejados. Genérico
    respecto de cuál dirección es la canónica -- para Nidir es
    "right"; para Bunny, este mismo patrón corre con "left" como
    canónica y deriva "right" -- la función en sí no lo asume."""
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

    # Misma política de canvas de trabajo compartido/anclado por
    # contenido que Nidir ya usa (Block 04.3) -- ver
    # prep_dev_sprite.compute_frame_normalization_plan() y el
    # docstring de tools/compile_pet_pack.py. "idle" es el grupo de
    # referencia por la misma convención que Nidir (DEC-045).
    normalization_entries = {
        "idle": prep_dev_sprite.read_png_rgba(os.path.join(IDLE_CANONICAL_FRAMES_DIR, "frame_000.png")),
        "idle_right": prep_dev_sprite.read_png_rgba(os.path.join(IDLE_DERIVED_FRAMES_DIR, "frame_000.png")),
        "click_reaction": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_CANONICAL_FRAMES_DIR, "frame_000.png")),
        "click_reaction_right": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_DERIVED_FRAMES_DIR, "frame_000.png")),
    }
    normalization_groups = {
        "idle": "idle",
        "idle_right": "idle",
        "click_reaction": "click_reaction",
        "click_reaction_right": "click_reaction",
    }
    normalization_plan = prep_dev_sprite.compute_frame_normalization_plan(
        normalization_entries, normalization_groups, reference_group="idle"
    )
    _, working_width, working_height, _, _ = normalization_plan["idle"]
    print(f"canvas de trabajo compartido (idle + click_reaction, contenido alineado): {working_width}x{working_height}")
    for key, (scale, _, _, offset_x, offset_y) in normalization_plan.items():
        print(f"  {key}: content_scale={scale:.4f} offset=({offset_x},{offset_y})")

    canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(working_width, working_height)
    print(
        f"canvas lógico derivado: {canvas_width}x{canvas_height} (canvas de trabajo {working_width}x{working_height}, "
        f"referencia {prep_dev_sprite.REFERENCE_LOGICAL_SIZE} x DISPLAY_SIZE_SCALE_FACTOR={prep_dev_sprite.DISPLAY_SIZE_SCALE_FACTOR})"
    )
    print(f"resolución de runtime (compilada): máximo {RUNTIME_MAX_FRAME_DIMENSION}px por lado, fuente sin tocar")

    left_dir = os.path.join("animations", "idle", "left", "frames")
    right_dir = os.path.join("animations", "idle", "right", "frames")
    idle_left_entries = [_frame_entry(left_dir, i) for i in range(idle_left_report.frame_count)]
    idle_right_entries = [_frame_entry(right_dir, i) for i in range(idle_right_report.frame_count)]

    click_left_dir = os.path.join("animations", "click_reaction", "left", "frames")
    click_right_dir = os.path.join("animations", "click_reaction", "right", "frames")
    click_left_entries = [_frame_entry(click_left_dir, i) for i in range(click_left_report.frame_count)]
    click_right_entries = [_frame_entry(click_right_dir, i) for i in range(click_right_report.frame_count)]

    idle_playback_fps = idle_left_report.frame_count / EXPORT_DURATION_SECONDS
    click_playback_fps = click_left_report.frame_count / EXPORT_DURATION_SECONDS
    print(f"idle: {idle_left_report.frame_count} frames reales, fps derivado={idle_playback_fps:.4f}")
    print(f"click_reaction: {click_left_report.frame_count} frames reales, fps derivado={click_playback_fps:.4f}")

    manifest = {
        # Se mantiene "bunny_dev" -- ver "Identidad de catálogo" en el
        # docstring del módulo.
        "id": "bunny_dev",
        "display_name": "Bunny",
        "variant_group": "",
        "canvas_width": canvas_width,
        "canvas_height": canvas_height,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "passive_interval_seconds": PASSIVE_INTERVAL_SECONDS,
        "content_version": "block04.3-bunny-1",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
        "normalize_visual_scale": True,
        # IMPORTANTE -- content::ResolveIdleAnimation()/
        # ResolveClickReaction()/ResolvePassiveAction() (sin cambios,
        # Block 04.2) SIEMPRE resuelven el campo CANÓNICO (este de
        # acá, sin override) para Direction::kRight -- un override
        # direccional solo se consulta para direcciones DISTINTAS de
        # kRight. Como el export real de Bunny es canónicamente
        # "left" (no "right", a diferencia de Nidir), el campo
        # canónico de abajo usa los frames DERIVADOS (espejados,
        # "right") y el override "left" usa los frames REALES
        # importados -- exactamente al revés de dónde vive cada uno en
        # disco (assets/source/.../left/ es la fuente real,
        # .../right/ es la derivada), pero es lo que el runtime
        # necesita para que Direction::kRight/kLeft resuelvan al
        # contenido visualmente correcto. Poner esto al revés sería un
        # bug real (Bunny se vería con la dirección equivocada en
        # kRight, el default inicial del controller) -- confirmado
        # contra el binario real antes de dar esto por bueno, ver el
        # informe de este bloque.
        #
        # Pose base: ESTÁTICA, un solo frame (frame_000 de idle) --
        # misma semántica que Nidir (Block 04.2, segunda pasada):
        # ningún deadline de frame mientras está en reposo.
        "idle": {
            "id": "idle_base_right",
            "kind": "static",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [_frame_entry(right_dir, 0)],
        },
        "idle_direction_overrides": [
            {
                "direction": "left",
                "id": "idle_base_left",
                "kind": "static",
                "fps": 0,
                "returns_to_idle": True,
                "frames": [_frame_entry(left_dir, 0)],
            }
        ],
        # click real (importado en este bloque) -- one_shot, vuelve a
        # la pose base estática al terminar
        # (AnimationController::TransitionToIdle()).
        "click_reaction": {
            "id": "click_right",
            "kind": "one_shot",
            "fps": click_playback_fps,
            "returns_to_idle": True,
            "frames": click_right_entries,
        },
        "click_reaction_direction_overrides": [
            {
                "direction": "left",
                "id": "click_left",
                "kind": "one_shot",
                "fps": click_playback_fps,
                "returns_to_idle": True,
                "frames": click_left_entries,
            }
        ],
        # Único idle periódico existente hoy (ver el docstring del
        # módulo -- una segunda animación de idle, 70/30, y hover
        # quedan deliberadamente fuera de alcance de este bloque).
        "passive_actions": [
            {
                "id": "idle_breathing_right",
                "kind": "one_shot",
                "fps": idle_playback_fps,
                "returns_to_idle": True,
                "frames": idle_right_entries,
            }
        ],
        "passive_action_direction_overrides": [
            {
                "passive_action_index": 0,
                "direction": "left",
                "id": "idle_breathing_left",
                "kind": "one_shot",
                "fps": idle_playback_fps,
                "returns_to_idle": True,
                "frames": idle_left_entries,
            }
        ],
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote manifest: {MANIFEST_PATH}")

    pet_id, total_bytes = compile_pet_pack.compile_pack(MANIFEST_PATH, COMPILED_PATH)
    print(f"compiled pet '{pet_id}': {COMPILED_PATH} ({total_bytes} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
