#!/usr/bin/env python3
"""Genera el contenido "left" de Nidir a partir de sus frames "right"
reales, ya importados en este repo (ver docs/NIDIR_CONTENT.md), y
compila el pack de runtime completo con la semántica real de producto:
pose base estática + animación de idle esporádica (one-shot) + click-
fire real (one-shot) — ver la corrección de Block 04.2 (segunda
pasada, "corrección de semántica de animación") y la tercera pasada
("import del click-fire real") en docs/DECISION_LOG.md.

Nidir es el primer Nimvlet con arte real de producción (a diferencia de
Bunny, que sigue siendo un fixture de QA -- ver AGENTS.md §11). Este
script NO inventa contenido nuevo: tanto los frames "right" de idle
(`assets/source/nimvlets/nidir/animations/idle/right/frames/`) como
los de click_reaction
(`assets/source/nimvlets/nidir/animations/click_reaction/right/frames/`)
ya existen en este repo, importados desde exports reales de Ludo.ai del
owner (idle: primera pasada de este bloque; click-fire: tercera pasada,
copiado desde `local_imports/nidir/nidir-click-fire-right/` una vez que
el bloqueo de acceso a `~/Downloads` se resolvió -- ver
docs/NIDIR_CONTENT.md §10). Lo único que este script deriva es la
dirección "left" de CADA animación, por espejado horizontal
determinista (block brief §3: "Do not use AI to regenerate the left
side") -- exactamente el mismo principio de "no depender de arte por
terminar/generar" que tools/generate_bunny_dev_pack.py estableció en
Block 02, aplicado acá a una dirección en vez de a una animación
completa.

Semántica real de producto (corregida en la segunda pasada de este
bloque -- ver el informe final):
    - `idle` (PetDefinition.idle) es la pose base ESTÁTICA (un solo
      frame, frame_000 de la secuencia importada) — lo que se muestra
      la mayor parte del tiempo, sin ningún deadline de frame (mismo
      comportamiento event-driven que Bunny ya demuestra desde
      Block 02).
    - La secuencia completa importada (todos los frames reales) pasa a
      ser un `passive_action` ONE-SHOT -- se dispara esporádicamente
      (reusa el scheduler de acciones pasivas ya existente desde
      Block 02, `passiveIntervalSeconds`/`TriggerPassiveAction`) y,
      al terminar, vuelve a la pose base estática exactamente por el
      mismo mecanismo que ya usan el click reaction de Bunny y de
      Nidir (`AnimationController::TransitionToIdle()`).
    - NUNCA se reproduce en loop continuo. No hay ningún cambio de
      arquitectura en `content::AnimationController` para lograr
      esto -- Bunny ya probaba exactamente este patrón
      (estático + one-shot) desde Block 02; el pack de Nidir de la
      primera pasada de este bloque lo clasificaba mal (`kind: loop`
      donde debía ser un `passive_action` `one_shot`).

Escribe, para CADA animación (idle, click_reaction):
    assets/source/nimvlets/nidir/animations/<anim>/left/frames/frame_NNN.png
        (espejo horizontal exacto de cada frame "right")
    assets/source/nimvlets/nidir/animations/<anim>/left/spritesheet/spritesheet.png
        (ensamblado desde los frames "left" ya espejados, en la misma
        grilla 5x5 que el spritesheet "right" original -- NUNCA espejar
        la imagen del spritesheet completo de una sola vez, porque eso
        invertiría también el ORDEN de las celdas en la grilla, no solo
        el contenido de cada una -- ver block brief §3: "generate a
        mirrored spritesheet only if safe/deterministic; otherwise
        generate it from normalized left frames")
Y además:
    assets/source/nimvlets/nidir/pack_manifest.json
        (entrada de tools/compile_pet_pack.py para Nidir)
    assets/dev/nidir_pack.nvpack
        (pack de runtime compilado -- lo que src/app carga de verdad)

Determinista: re-correr este script contra los mismos frames "right"
(idle y click_reaction) produce salida byte-idéntica. Este script NO
toca `local_imports/` -- esa carpeta es staging temporal del owner
(nunca commiteada, ver .gitignore) y el copiado desde ahí hacia
`assets/source/nimvlets/nidir/animations/click_reaction/right/` ya se
hizo una única vez, de forma manual y documentada en el commit que
importó el click-fire real (mismo patrón que el import original de
idle -- ver `git log` de ese commit: "zero actual normalization"
needed, así que no hizo falta ningún script de importación dedicado,
solo un `cp`).

Block 04.3 (QA manual del owner encontró clipping, pérdida de calidad
y tamaño visual inconsistente entre idle y click-fire -- ver
docs/NIDIR_CONTENT.md, "clipping y tamaño visual inconsistente entre
animaciones") agrega dos cambios acá:
    - El manifest ahora pide `normalize_visual_scale: true` --
      tools/compile_pet_pack.py deriva, a partir de los pixeles reales
      de idle y click_reaction, un canvas de trabajo compartido que
      alinea el personaje al mismo tamaño/posición en ambas
      animaciones (ver prep_dev_sprite.compute_frame_normalization_plan()).
      `canvas_width`/`canvas_height` (el tamaño LÓGICO en pantalla) se
      derivan de ESE canvas de trabajo compartido, no solo de la
      resolución nativa de idle como en la segunda pasada -- así el
      canvas lógico ya contempla el encuadre más ancho que necesita el
      efecto de fuego.
    - `PASSIVE_INTERVAL_SECONDS_PLACEHOLDER` pasa de 300s (5min) a 60s
      (1min) -- ahora es un valor de producto explícitamente pedido
      por el owner para este bloque, no un placeholder.

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

# Click-fire real (importado en la tercera pasada de este bloque desde
# local_imports/nidir/nidir-click-fire-right/, ya copiado a estas
# rutas canónicas -- ver el commit de import). Mismo layout que idle,
# una animación distinta bajo animations/.
CLICK_ROOT = os.path.join(NIDIR_ROOT, "animations", "click_reaction")
CLICK_RIGHT_FRAMES_DIR = os.path.join(CLICK_ROOT, "right", "frames")
CLICK_LEFT_FRAMES_DIR = os.path.join(CLICK_ROOT, "left", "frames")
CLICK_RIGHT_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "right", "spritesheet", "spritesheet.png")
CLICK_LEFT_SPRITESHEET_PATH = os.path.join(CLICK_ROOT, "left", "spritesheet", "spritesheet.png")

MANIFEST_PATH = os.path.join(NIDIR_ROOT, "pack_manifest.json")
COMPILED_PATH = os.path.join(REPO_ROOT, "assets", "dev", "nidir_pack.nvpack")

# Tamaño de canvas LÓGICO (puntos en pantalla, ver SDL_CreateWindow en
# src/app/SpikeApp.cpp) vs. resolución de PIXELES almacenada en el pack
# compilado -- dos preocupaciones deliberadamente separadas (política
# genérica, ver tools/prep_dev_sprite.py's compute_logical_canvas_size()/
# resize_rgba_nearest() y docs/NIDIR_CONTENT.md, "tamaño de canvas
# lógico vs. resolución de frame"):
#   - El canvas lógico se deriva del aspect ratio nativo de Nidir
#     (513x525) escalado a la misma clase "160" que Bunny ya usa desde
#     Block 02 -- NO la resolución nativa 1:1 (ese fue el bug real de
#     la primera pasada de este bloque: Nidir aparecía mucho más
#     grande que Bunny en pantalla).
#   - `RUNTIME_MAX_FRAME_DIMENSION` limita la resolución de pixeles que
#     efectivamente termina en el pack compilado (y por lo tanto en
#     RAM/textura en runtime) -- independiente del canvas lógico, pero
#     con margen suficiente para verse nítido a 2x densidad de pixel
#     (Retina) sin almacenar muchísimo más detalle del que cualquier
#     pantalla real puede mostrar. 2x el tamaño de referencia (160) es
#     ese margen. Los PNG fuente en assets/source/ NUNCA se tocan --
#     esto solo afecta los bytes que van al .nvpack.
#
# CANVAS_WIDTH/CANVAS_HEIGHT se calculan más abajo, dentro de main(),
# a partir de la resolución nativa REAL medida de los frames right
# (right_report.width/height) -- nunca hardcodeados acá, para no
# repetir "513x525" como constante mágica separada de lo que
# validate_frame_sequence.py efectivamente encontró.
RUNTIME_MAX_FRAME_DIMENSION = 2 * prep_dev_sprite.REFERENCE_LOGICAL_SIZE

# Grilla del spritesheet: 5 columnas x 5 filas, orden row-major
# (frame 0 arriba-izquierda, avanzando por fila) -- confirmado
# inspeccionando visualmente assets/source/nimvlets/nidir/animations/idle/right/spritesheet/spritesheet.png
# (2565x2625 == 5*513 x 5*525 exacto) antes de escribir este código, no
# asumido.
GRID_COLUMNS = 5

# 128: mismo punto medio estándar de borde antialiaseado que Bunny usa
# (docs/DECISION_LOG.md DEC-018) -- confirmado con el histograma real
# de frame_000 de Nidir, no reusado a ciegas: fondo en alpha=0 exacto
# (50.84%), interior agrupado en alpha 254-255 (47.41%), y solo un
# 1.75% de pixeles en la banda de borde antialiaseado entre medio,
# donde el umbral realmente importa. Ver docs/NIDIR_CONTENT.md.
ALPHA_HIT_THRESHOLD = 128

# Duración de generación configurada del lado de Ludo.ai para el
# export de idle (dato real, provisto por el owner en la segunda
# pasada de este bloque -- "Our current Ludo generation setup is: 3
# seconds, Max Frames 25" -- no una medición ni una suposición de este
# repo). El fps de reproducción se deriva de esto (frame_count real /
# 3.0s), NUNCA de un frame_count hardcodeado -- la cantidad real de
# frames que entrega cada export puede variar (ver
# validate_frame_sequence.py, que nunca fuerza 24/25 frames). Como la
# secuencia es un passive_action/click_reaction ESPORÁDICO (nunca un
# loop continuo), esta cadencia no necesita balancearse contra un costo
# de CPU permanente -- solo importa mientras el one-shot está
# efectivamente reproduciéndose.
#
# El export de click-fire (tercera pasada de este bloque) también trae
# 25 frames -- el mismo conteo que idle bajo la misma configuración de
# "Max Frames 25" de Ludo. Se reusa esta misma constante para derivar
# su fps, asumiendo la misma duración de generación de 3s -- una
# suposición explícita y documentada (el owner no confirmó
# separadamente la duración de este export puntual), no un dato
# medido de forma independiente. Si en el futuro se confirma que el
# click-fire usó una duración distinta, solo hay que ajustar
# CLICK_EXPORT_DURATION_SECONDS abajo -- ya está desacoplada de
# EXPORT_DURATION_SECONDS a propósito, aunque hoy compartan el mismo
# valor.
EXPORT_DURATION_SECONDS = 3.0
CLICK_EXPORT_DURATION_SECONDS = 3.0

# Cada cuánto se dispara la animación de idle esporádica. Block 04.2
# lo dejaba en 300s (5min, el default del esquema de PetDefinition) de
# forma explícita como no-final -- Block 04.3 lo fija a 60s (1 minuto)
# por pedido directo del owner ("El idle periódico queremos dejarlo en
# 1 minuto"), ya no un placeholder sino un valor de producto real.
PASSIVE_INTERVAL_SECONDS = 60.0


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


def _derive_left_direction(
    label: str,
    right_frames_dir: str,
    left_frames_dir: str,
    right_spritesheet_path: str,
    left_spritesheet_path: str,
) -> tuple[validate_frame_sequence.FrameSequenceReport, validate_frame_sequence.FrameSequenceReport]:
    """Valida los frames "right" reales ya importados de una animación,
    deriva su dirección "left" por espejado horizontal determinista, la
    valida contra el mismo contrato, y ensambla su spritesheet "left"
    desde los frames ya espejados (nunca espejando la imagen completa
    del spritesheet -- ver el docstring del módulo). Reusado tanto para
    `idle` como para `click_reaction` -- misma lógica, dos animaciones
    distintas."""
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
    # El espejado preserva dimensiones y transparencia exactamente --
    # confirmar que el reporte de left calza con el de right, no solo
    # confiar en que "debería".
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

    # Canvas lógico derivado del canvas de TRABAJO compartido (Block
    # 04.3), no de la resolución nativa cruda de idle (eso era la
    # segunda pasada) -- ver prep_dev_sprite.compute_frame_normalization_plan()
    # y su uso idéntico dentro de tools/compile_pet_pack.py
    # (`normalize_visual_scale`, más abajo en el manifest). Se computa
    # acá SOLO para derivar canvas_width/canvas_height con el aspect
    # ratio correcto -- compile_pet_pack.py vuelve a calcular el mismo
    # plan de forma independiente a partir de los mismos PNG fuente
    # (no se pasa ningún estado entre los dos scripts), así que ambos
    # SIEMPRE coinciden sin necesidad de sincronizar nada.
    normalization_entries = {
        "idle": prep_dev_sprite.read_png_rgba(os.path.join(RIGHT_FRAMES_DIR, "frame_000.png")),
        "idle_left": prep_dev_sprite.read_png_rgba(os.path.join(LEFT_FRAMES_DIR, "frame_000.png")),
        "click_reaction": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_RIGHT_FRAMES_DIR, "frame_000.png")),
        "click_reaction_left": prep_dev_sprite.read_png_rgba(os.path.join(CLICK_LEFT_FRAMES_DIR, "frame_000.png")),
    }
    normalization_groups = {
        "idle": "idle",
        "idle_left": "idle",
        "click_reaction": "click_reaction",
        "click_reaction_left": "click_reaction",
    }
    normalization_plan = prep_dev_sprite.compute_frame_normalization_plan(
        normalization_entries, normalization_groups, reference_group="idle"
    )
    _, working_width, working_height, _, _ = normalization_plan["idle"]
    print(f"canvas de trabajo compartido (idle + click_reaction, contenido alineado): {working_width}x{working_height}")
    for key, (scale, _, _, offset_x, offset_y) in normalization_plan.items():
        print(f"  {key}: content_scale={scale:.4f} offset=({offset_x},{offset_y})")

    canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(working_width, working_height)
    print(f"canvas lógico derivado: {canvas_width}x{canvas_height} (canvas de trabajo {working_width}x{working_height}, referencia {prep_dev_sprite.REFERENCE_LOGICAL_SIZE})")
    print(f"resolución de runtime (compilada): máximo {RUNTIME_MAX_FRAME_DIMENSION}px por lado, fuente sin tocar")

    right_dir = os.path.join("animations", "idle", "right", "frames")
    left_dir = os.path.join("animations", "idle", "left", "frames")
    right_frame_entries = [_frame_entry(right_dir, i) for i in range(right_report.frame_count)]
    left_frame_entries = [_frame_entry(left_dir, i) for i in range(left_report.frame_count)]

    click_right_dir = os.path.join("animations", "click_reaction", "right", "frames")
    click_left_dir = os.path.join("animations", "click_reaction", "left", "frames")
    click_right_frame_entries = [_frame_entry(click_right_dir, i) for i in range(click_right_report.frame_count)]
    click_left_frame_entries = [_frame_entry(click_left_dir, i) for i in range(click_left_report.frame_count)]

    # fps derivado de la cantidad REAL de frames entregados / la
    # duración de generación configurada en Ludo.ai -- nunca
    # hardcodeado a 24/25 (ver EXPORT_DURATION_SECONDS arriba y
    # validate_frame_sequence.py).
    idle_playback_fps = right_report.frame_count / EXPORT_DURATION_SECONDS
    click_playback_fps = click_right_report.frame_count / CLICK_EXPORT_DURATION_SECONDS
    print(f"click_reaction: {click_right_report.frame_count} frames reales @ {click_right_report.width}x{click_right_report.height}, fps derivado={click_playback_fps:.4f}")

    manifest = {
        "id": "nidir",
        "display_name": "Nidir",
        "variant_group": "",
        "canvas_width": canvas_width,
        "canvas_height": canvas_height,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "passive_interval_seconds": PASSIVE_INTERVAL_SECONDS,
        "content_version": "block04.3-nidir-1",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
        # Canvas de trabajo compartido, anclado por contenido (Block
        # 04.3 -- ver docs/NIDIR_CONTENT.md, "clipping y tamaño visual
        # inconsistente entre animaciones", y el docstring de
        # tools/compile_pet_pack.py). Sin esto, idle y click_reaction
        # se estiraban cada uno de forma independiente al mismo canvas
        # lógico fijo, mostrando al personaje a tamaños/posiciones
        # distintos según qué animación estuviera activa.
        "normalize_visual_scale": True,
        # Pose base: ESTÁTICA, un solo frame (frame_000 -- la misma
        # imagen que también es el último frame lógico de la
        # animación esporádica de abajo, ver "first/last frame
        # contract" en el informe final para el hallazgo real sobre
        # qué tan parecidos son frame_000 y el último frame
        # efectivamente exportado). Sin esto, PlaybackKind::kStatic
        # nunca tiene ningún deadline de frame -- exactamente el
        # comportamiento event-driven que Bunny ya demuestra.
        "idle": {
            "id": "idle_base",
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
        # click-fire real (importado en la tercera pasada de este
        # bloque, ver docs/NIDIR_CONTENT.md §10/§11) -- reemplaza el
        # placeholder estructural de un solo frame de las pasadas
        # anteriores. 25 frames reales exportados de Ludo.ai
        # ("nidir-click-fire-right"), one_shot, mismo mecanismo de
        # retorno a la pose base estática que ya usa passive_actions
        # (returns_to_idle vía AnimationController::TransitionToIdle()).
        # Debe ser one_shot (no static): un click_reaction static
        # dejaría al AnimationController trabado en ese estado para
        # siempre, ya que Advance() nunca transiciona de vuelta a Idle
        # para una animación kStatic.
        "click_reaction": {
            "id": "click_fire_right",
            "kind": "one_shot",
            "fps": click_playback_fps,
            "returns_to_idle": True,
            "frames": click_right_frame_entries,
        },
        "click_reaction_direction_overrides": [
            {
                "direction": "left",
                "id": "click_fire_left",
                "kind": "one_shot",
                "fps": click_playback_fps,
                "returns_to_idle": True,
                "frames": click_left_frame_entries,
            }
        ],
        # La secuencia completa importada, reclasificada como acción
        # pasiva ESPORÁDICA (one_shot) -- nunca un loop continuo. Se
        # dispara vía el scheduler de acciones pasivas ya existente
        # desde Block 02 (SpikeApp::nextPassiveDeadlineMs_ /
        # AnimationController::TriggerPassiveAction()), y al terminar
        # vuelve a la pose base estática por el mismo mecanismo que ya
        # usa cualquier click_reaction (returnsToIdle).
        "passive_actions": [
            {
                "id": "idle_breathing_right",
                "kind": "one_shot",
                "fps": idle_playback_fps,
                "returns_to_idle": True,
                "frames": right_frame_entries,
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
                "frames": left_frame_entries,
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
