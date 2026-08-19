#!/usr/bin/env python3
"""Genera el contenido "left" de Nidir a partir de sus frames "right"
reales, ya importados en este repo (ver docs/NIDIR_CONTENT.md), y
compila el pack de runtime completo con la semántica real de producto:
pose base estática + animación de idle esporádica (one-shot) + click
(one-shot) — ver la corrección de Block 04.2 (segunda pasada,
"corrección de semántica de animación") en docs/DECISION_LOG.md.

Nidir es el primer Nimvlet con arte real de producción (a diferencia de
Bunny, que sigue siendo un fixture de QA -- ver AGENTS.md §11). Este
script NO inventa contenido nuevo: los frames "right" ya existen en
`assets/source/nimvlets/nidir/animations/idle/right/frames/`
(importados desde el export real de Ludo.ai del owner). Lo único que
este script deriva es la dirección "left", por espejado horizontal
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

Escribe:
    assets/source/nimvlets/nidir/animations/idle/left/frames/frame_NNN.png
        (espejo horizontal exacto de cada frame "right")
    assets/source/nimvlets/nidir/animations/idle/left/spritesheet/spritesheet.png
        (ensamblado desde los frames "left" ya espejados, en la misma
        grilla 5x5 que el spritesheet "right" original -- NUNCA espejar
        la imagen del spritesheet completo de una sola vez, porque eso
        invertiría también el ORDEN de las celdas en la grilla, no solo
        el contenido de cada una -- ver block brief §3: "generate a
        mirrored spritesheet only if safe/deterministic; otherwise
        generate it from normalized left frames")
    assets/source/nimvlets/nidir/pack_manifest.json
        (entrada de tools/compile_pet_pack.py para Nidir)
    assets/dev/nidir_pack.nvpack
        (pack de runtime compilado -- lo que src/app carga de verdad)

Determinista: re-correr este script contra los mismos frames "right"
produce salida byte-idéntica.

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

# Duración de generación configurada del lado de Ludo.ai para este
# export (dato real, provisto por el owner en esta segunda pasada del
# bloque -- "Our current Ludo generation setup is: 3 seconds, Max
# Frames 25" -- no una medición ni una suposición de este repo). El fps
# de reproducción se deriva de esto (frame_count real / 3.0s), NUNCA
# de un frame_count hardcodeado -- la cantidad real de frames que
# entrega cada export puede variar (ver validate_frame_sequence.py, que
# nunca fuerza 24/25 frames). Como la secuencia ahora es un
# passive_action ESPORÁDICO (no un loop continuo), esta cadencia ya no
# necesita balancearse contra un costo de CPU permanente -- solo
# importa mientras el one-shot está efectivamente reproduciéndose, unas
# pocas veces por hora como mucho.
EXPORT_DURATION_SECONDS = 3.0

# Placeholder de producto: cada cuánto se dispara la animación de idle
# esporádica. La cadencia real (1/3/5 minutos, etc.) es política de
# producto, NO decidida en este bloque -- se deja el mismo default de
# 300s (5 min) que el esquema de PetDefinition ya usaba, documentado
# explícitamente como no-final.
PASSIVE_INTERVAL_SECONDS_PLACEHOLDER = 300.0


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


def main() -> int:
    right_report = validate_frame_sequence.validate_frame_sequence(RIGHT_FRAMES_DIR, ALPHA_HIT_THRESHOLD)
    print(f"right frames válidos: {right_report.frame_count} @ {right_report.width}x{right_report.height}")

    canvas_width, canvas_height = prep_dev_sprite.compute_logical_canvas_size(right_report.width, right_report.height)
    print(f"canvas lógico derivado: {canvas_width}x{canvas_height} (nativo {right_report.width}x{right_report.height}, referencia {prep_dev_sprite.REFERENCE_LOGICAL_SIZE})")
    print(f"resolución de runtime (compilada): máximo {RUNTIME_MAX_FRAME_DIMENSION}px por lado, fuente sin tocar")

    if not os.path.isfile(RIGHT_SPRITESHEET_PATH):
        print(f"error: falta el spritesheet right importado: {RIGHT_SPRITESHEET_PATH}", file=sys.stderr)
        return 1

    os.makedirs(LEFT_FRAMES_DIR, exist_ok=True)
    os.makedirs(os.path.dirname(LEFT_SPRITESHEET_PATH), exist_ok=True)

    for index in range(right_report.frame_count):
        src_path = os.path.join(RIGHT_FRAMES_DIR, f"frame_{index:03d}.png")
        w, h, pixels = prep_dev_sprite.read_png_rgba(src_path)
        mirrored = prep_dev_sprite.mirror_rgba_horizontal(w, h, pixels)
        dst_path = os.path.join(LEFT_FRAMES_DIR, f"frame_{index:03d}.png")
        prep_dev_sprite.write_png_rgba(dst_path, w, h, mirrored)

    left_report = validate_frame_sequence.validate_frame_sequence(LEFT_FRAMES_DIR, ALPHA_HIT_THRESHOLD)
    print(f"left frames generados y validados: {left_report.frame_count} @ {left_report.width}x{left_report.height}")
    # El espejado preserva dimensiones y transparencia exactamente --
    # confirmar que el reporte de left calza con el de right, no solo
    # confiar en que "debería".
    assert left_report.frame_count == right_report.frame_count
    assert (left_report.width, left_report.height) == (right_report.width, right_report.height)
    assert left_report.transparent_fraction_range == right_report.transparent_fraction_range, (
        "el espejado horizontal no debería alterar la fracción de pixeles transparentes de ningún frame"
    )

    sheet_w, sheet_h, sheet_pixels = _assemble_spritesheet_from_frames(
        LEFT_FRAMES_DIR, left_report.frame_count, left_report.width, left_report.height
    )
    prep_dev_sprite.write_png_rgba(LEFT_SPRITESHEET_PATH, sheet_w, sheet_h, sheet_pixels)
    print(f"left spritesheet ensamblado: {LEFT_SPRITESHEET_PATH} ({sheet_w}x{sheet_h})")

    def frame_entry(rel_dir: str, index: int) -> dict:
        return {"source": os.path.join(rel_dir, f"frame_{index:03d}.png"), "duration_ms": 0}

    right_dir = os.path.join("animations", "idle", "right", "frames")
    left_dir = os.path.join("animations", "idle", "left", "frames")
    right_frame_entries = [frame_entry(right_dir, i) for i in range(right_report.frame_count)]
    left_frame_entries = [frame_entry(left_dir, i) for i in range(left_report.frame_count)]

    # fps derivado de la cantidad REAL de frames entregados / la
    # duración de generación configurada en Ludo.ai -- nunca
    # hardcodeado a 24/25 (ver EXPORT_DURATION_SECONDS arriba y
    # validate_frame_sequence.py).
    idle_playback_fps = right_report.frame_count / EXPORT_DURATION_SECONDS

    manifest = {
        "id": "nidir",
        "display_name": "Nidir",
        "variant_group": "",
        "canvas_width": canvas_width,
        "canvas_height": canvas_height,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        "passive_interval_seconds": PASSIVE_INTERVAL_SECONDS_PLACEHOLDER,
        "content_version": "block04.2-nidir-3",
        "runtime_max_frame_dimension": RUNTIME_MAX_FRAME_DIMENSION,
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
            "frames": [frame_entry(right_dir, 0)],
        },
        "idle_direction_overrides": [
            {
                "direction": "left",
                "id": "idle_base_left",
                "kind": "static",
                "fps": 0,
                "returns_to_idle": True,
                "frames": [frame_entry(left_dir, 0)],
            }
        ],
        # Placeholder estructural: Nidir no tiene todavía arte de click
        # dedicado real (bloqueado en esta sesión -- ver el informe
        # final, "blocker de acceso a ~/Downloads") -- reutiliza el
        # primer frame de idle-right/idle-left como un "blip" de un
        # solo frame que vuelve a Idle casi de inmediato, en vez de
        # una animación real. Documentado explícitamente como
        # placeholder, NO removido todavía (removerlo sin el
        # reemplazo real dejaría click_reaction sin contenido válido,
        # una regresión peor que mantener el placeholder). Debe ser
        # one_shot (no static): un click_reaction static dejaría al
        # AnimationController trabado en ese estado para siempre, ya
        # que Advance() nunca transiciona de vuelta a Idle para una
        # animación kStatic.
        "click_reaction": {
            "id": "click_reaction_placeholder",
            "kind": "one_shot",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [frame_entry(right_dir, 0) | {"duration_ms": 100}],
        },
        "click_reaction_direction_overrides": [
            {
                "direction": "left",
                "id": "click_reaction_placeholder_left",
                "kind": "one_shot",
                "fps": 0,
                "returns_to_idle": True,
                "frames": [frame_entry(left_dir, 0) | {"duration_ms": 100}],
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
