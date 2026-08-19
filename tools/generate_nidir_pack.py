#!/usr/bin/env python3
"""Genera el contenido "left" de Nidir (Block 04.2) a partir de sus
frames "right" reales, ya importados en este repo (ver
docs/NIDIR_CONTENT.md), y compila el pack de runtime completo.

Nidir es el primer Nimvlet con arte real de producción (a diferencia de
Bunny, que sigue siendo un fixture de QA -- ver AGENTS.md §11). Este
script NO inventa contenido nuevo: los 25 frames "right" ya existen en
`assets/source/nimvlets/nidir/animations/idle/right/frames/`
(importados desde el export real de Ludo.ai del owner). Lo único que
este script deriva es la dirección "left", por espejado horizontal
determinista (block brief §3: "Do not use AI to regenerate the left
side") -- exactamente el mismo principio de "no depender de arte por
terminar/generar" que tools/generate_bunny_dev_pack.py estableció en
Block 02, aplicado acá a una dirección en vez de a una animación
completa.

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

# El canvas coincide exactamente con la resolución nativa de los frames
# importados -- a diferencia de Bunny (reescalado a 160x160 como
# fixture de dev), el brief de este bloque pide explícitamente no
# recortar/reescalar/recentrar salvo que el contrato de runtime lo
# exija (§4), y no lo exige: SDL_RenderTexture ya escala cualquier
# frame al canvas del pet sea cual sea su resolución nativa (ver
# src/app/SpikeApp.cpp), así que usar la resolución nativa 1:1 evita
# cualquier transformación de reescalado innecesaria en la fuente.
CANVAS_WIDTH = 513
CANVAS_HEIGHT = 525

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

# fps de la animación de idle: el export no trae ninguna cadencia
# indicada -- esta es una decisión de este bloque (documentada en
# docs/DECISION_LOG.md como decisión fuera del prompt), no un dato
# provisto por Ludo.ai. Se midió contra el binario Release real antes
# de fijar el valor (ver docs/PERFORMANCE_BUDGETS.md): a 12fps el idle
# loop de Nidir promedia ~11-12% CPU en steady state (canvas nativo
# 513x525, ~10x la cantidad de pixeles de Bunny); a 6fps baja a ~5%.
# 25 frames a 6fps = un loop de ~4.17s -- se prefirió el costo de CPU
# más bajo sobre una cadencia de reproducción más fluida, ya que este
# bloque no tiene ningún requisito de "fluidez" específico y sí tiene
# uno de recursos (block brief §10).
IDLE_FPS = 6.0


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

    right_frame_entries = [frame_entry(os.path.join("animations", "idle", "right", "frames"), i) for i in range(right_report.frame_count)]
    left_frame_entries = [frame_entry(os.path.join("animations", "idle", "left", "frames"), i) for i in range(left_report.frame_count)]

    manifest = {
        "id": "nidir",
        "display_name": "Nidir",
        "variant_group": "",
        "canvas_width": CANVAS_WIDTH,
        "canvas_height": CANVAS_HEIGHT,
        "alpha_hit_threshold": ALPHA_HIT_THRESHOLD,
        # Sin acción pasiva propia todavía (sin arte fuente para eso en
        # este bloque) -- 300.0 nunca se usa en la práctica mientras
        # passive_actions esté vacío, se deja el mismo default que
        # Bunny por consistencia de esquema.
        "passive_interval_seconds": 300.0,
        "content_version": "block04.2-nidir-1",
        "idle": {
            "id": "idle_right",
            "kind": "loop",
            "fps": IDLE_FPS,
            "returns_to_idle": True,
            "frames": right_frame_entries,
        },
        # Placeholder estructural: Nidir no tiene todavía arte de click
        # dedicado (el export del owner solo cubre idle) -- reutiliza el
        # primer frame de idle-right como un "blip" de un solo frame
        # que vuelve a Idle casi de inmediato, en vez de una animación
        # real. Documentado explícitamente como placeholder, no como
        # contenido terminado -- ver docs/NIDIR_CONTENT.md y
        # docs/DECISION_LOG.md. Debe ser one_shot (no static): un
        # click_reaction static dejaría al AnimationController
        # trabado en ese estado para siempre, ya que Advance() nunca
        # transiciona de vuelta a Idle para una animación kStatic.
        "click_reaction": {
            "id": "click_reaction_placeholder",
            "kind": "one_shot",
            "fps": 0,
            "returns_to_idle": True,
            "frames": [
                {"source": os.path.join("animations", "idle", "right", "frames", "frame_000.png"), "duration_ms": 100},
            ],
        },
        "passive_actions": [],
        "idle_direction_overrides": [
            {
                "direction": "left",
                "id": "idle_left",
                "kind": "loop",
                "fps": IDLE_FPS,
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
