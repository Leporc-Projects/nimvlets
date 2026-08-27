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

Direcciones de PROVENANCE (de los nombres de export FINALES del owner --
qué carpeta contiene qué orientación REAL, nunca renombrada):
    macho: LEFT (frin-macho-*-left)
    hembra: RIGHT (frin-hembra-*-right)
Mismo cuidado de inversión real/derivado que ya usa
tools/generate_bunny_pack.py.

Direcciones RUNTIME (pasada de pulido final -- ver DEC-091 en
docs/DECISION_LOG.md): INVERTIDAS respecto de la provenance de arriba,
a pedido explícito del owner. `Direction::kRight` en tiempo de
ejecución muestra los frames DE PROVENANCE "left" de cada variante, y
`Direction::kLeft` muestra los de provenance "right" -- lo opuesto de
lo que un mapeo directo (sin inversión) habría dado. Ver
`_invert_runtime_direction()` dentro de `_build_variant_manifest()`,
el único lugar donde se aplica esta inversión -- ver
_build_variant_manifest().

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

# Cuánto reposo GENUINO necesita Frin sentado antes de acostarse
# (`seated` --sit_to_lie--> `lying`). Historial: 45.0 (DEC-066, nunca
# confirmado por el owner) -> 15.0 (DEC-074) -> 12.0 (DEC-084,
# unificado con Bunny/Nidir) -> 10.0 (DEC-089, pasada de estabilización,
# pedido de producto explícito) -> 12.0 (pasada de continuidad de
# frontera, pedido de producto explícito -- revierte DEC-089's 10.0,
# vuelve a unificarse con Bunny/Nidir).
#
# Las semánticas de reset no cambian: click, hover, drag y la
# terminación de cualquier acción reinician la cuenta desde ese
# instante (ver SpikeApp::RearmAmbientDeadline()). `lying` sigue sin
# ambient_actions, así que nunca hay timer armado ahí. El valor sigue
# siendo un dato de CONTENIDO por-estado
# (BehaviorState::ambientIntervalSeconds), nunca hardcodeado por
# especie -- que hoy coincida numéricamente con Bunny/Nidir es una
# decisión de producto, no una restricción del modelo: Artu (futuro,
# misma forma de grafo) define el suyo sin tocar código, y este mismo
# número puede volver a diferenciarse sin tocar una sola línea de
# motor.
REST_DELAY_SECONDS = 12.0

# 70/30 entre howl y tail_greet -- mismo mecanismo genérico que el
# ambient 70/30 de Bunny/Nidir (content::ChooseWeightedActionIndex()),
# acá aplicado al trigger de click en vez de ambient.
SEATED_CLICK_WEIGHTS = {"howl": 0.7, "tail_greet": 0.3}

# Corrección posicional TERMINAL de `lie_to_sit` (pasada de corrección
# posicional final -- ver `terminal_rigid_translation` en
# tools/compile_pet_pack.py, DEC-105). Reemplaza la sustitución de cola
# (`stable_pose_tail_frames`, DEC-101): en vez de reemplazar los frames
# finales por copias congeladas de la base, se TRASLADAN los pixeles
# AUTORADOS a la posición correcta -- misma animación real, compuesta
# en otro lugar del canvas compartido. Solo el frame absolutamente
# final sigue siendo sustitución exacta (contrato semántico de punta,
# DEC-099, sin cambios).
#
# `start_frame` es el mismo K re-derivado en la pasada anterior (misma
# búsqueda exhaustiva, mismo criterio -- minimizar distancia de
# centroide a la base sentada sujeto a que el paso de entrada al último
# frame ANTES del corte sea > 0.5px, para no cortar sobre un frame ya
# inmóvil). Matemáticamente equivalente: anclar el frame K exactamente
# sobre la base sentada dando (dx, dy) = base - centroide(frame K)
# produce, en el borde K-1 -> K, el MISMO salto que sustituir K
# directamente por la base -- la traslación no reduce esa magnitud
# (piso geométrico real, medido en la pasada anterior: macho ~24.46px,
# irreducible; hembra ~5.32px). Lo que SÍ cambia es qué pixeles se
# muestran desde K en adelante: la animación autorada real (por chica
# que sea su variación en esta cola -- pasos <=1px), en vez de una
# pose congelada repetida.
#
# `dx`/`dy` en pixeles del frame COMPILADO (esta resolución, tras el
# downscale de runtime), medidos por separado para CADA dirección
# runtime sobre el pack ya compilado -- nunca una derivada y la otra
# negada por suposición de espejo (macho: derecha +23.44/-6.33,
# izquierda -23.52/-6.33 -- casi exactos pero NO idénticos, la
# asimetría real del contenido). Ver el informe de este bloque para la
# tabla completa antes/después.
LIE_TO_SIT_TERMINAL_TRANSLATION = {
    "male": {
        "right": (17, 23.444248400828172, -6.3267841749587035),
        "left": (17, -23.516887190412987, -6.325219572337261),
    },
    "female": {
        "right": (21, -1.7148204938495581, -5.111141094585719),
        "left": (21, 1.503223680930546, -5.095938161702634),
    },
}

# `tail_greet` del MACHO viene ~2.5% más oscuro que la pose sentada YA
# EN EL PNG FUENTE (medido sobre pixeles interiores, alpha>=250, así que
# no es un efecto de borde ni de premultiplicado; el pipeline de
# compilación se midió fiel a +0.15%). La hembra NO tiene el problema
# (-0.5%, dentro del ruido), así que no se le aplica nada: corregir ahí
# sería resamplear para nada. Ver DEC-102.
TAIL_GREET_COLOR_MATCH = {"male": True, "female": False}

# `howl` es el peor caso de desacuerdo de aspecto de export del
# proyecto: -3.90% (macho) y -2.1% (hembra), idéntico en sus DOS puntas
# de reposo. `tail_greet` NO se corrige: medido +0.20% (macho) y -0.74%
# (hembra), y su geometría está explícitamente aprobada por QA -- es el
# control negativo dentro de Frin. Ver DEC-100.

# Escala visual por-pet (Block 05 -- ver content::PetDefinition::
# visualScale). DEC-076 fijó 1.30 tras QA manual real ("Frin
# male/female currently feel too small; visibly larger, comparable in
# presence to Bunny/Nidir"). Ese TAMAÑO EN PANTALLA sigue aprobado y no
# cambia; lo que cambió en la pasada de estabilización es el
# denominador del que se deriva.
#
# Por qué el número bajó a 1.05 sin que Frin se vea más chico
# (DEC-087): al arreglar la continuidad de las transiciones, la pose
# base de `lying` dejó de exigir su propio centrado y pasó a heredar la
# colocación de `sit_to_lie`. Eso saca margen transparente muerto del
# canvas de trabajo compartido -- macho 543x815 -> 546x657, hembra
# 496x653 -> 495x531 -- así que el canvas lógico derivado pasa de
# 117x176 a 146x176 (macho) y de 134x176 a 163x176 (hembra), y el mismo
# personaje ocupa una fracción mucho mayor de él.
#
# El valor se DERIVA, no se tantea. El alto de contenido en pantalla es
#   (alto_contenido_nativo * content_scale / alto_canvas_trabajo) * alto_logico * visual_scale
# y se resuelve para dejarlo igual que antes:
#   macho:  (593/815)*176*1.30 = 166.5pt  ->  (593/657)*176*V  ->  V = 1.048
#   hembra: (513/653)*176*1.30 = 179.7pt  ->  (513/531)*176*V  ->  V = 1.057
# 1.05 queda a menos de 1% de ambos, y sigue siendo UN SOLO valor para
# las dos variantes (mismo personaje, mismo tamaño percibido esperado,
# sin importar el género) -- trivial de ajustar: cambiar este único
# número y volver a correr este script.
#
# Efecto secundario real y bienvenido: con el canvas más ajustado, el
# tope de `runtime_max_frame_dimension` (320) recorta menos -- el ratio
# de downscale del macho pasa de 0.393 a 0.487, o sea que Frin se
# compila a ~24% más resolución efectiva para el mismo tamaño en
# pantalla.
VISUAL_SCALE = 1.05

# Escala visual POR-VARIANTE (pasada de pulido final -- pedido de
# producto explícito: "Frin male +5%, female sin cambio"). Generaliza
# el VISUAL_SCALE único de arriba -- que sigue siendo la base común
# derivada de la correspondencia de tamaño con Bunny/Nidir -- a un
# multiplicador propio por variante, sin tocar absolutamente nada más
# de la geometría (ni contenido, ni content_scale, ni canvas de
# trabajo): visual_scale se aplica exclusivamente en runtime
# (SpikeApp::EffectiveCanvasWidth()/Height()), así que es el único
# lugar correcto para una diferencia de tamaño puramente de producto
# entre las dos variantes.
#
# female se queda en 1.0 -- "female stays exactly the current size" --
# así que su visual_scale efectivo sigue siendo VISUAL_SCALE sin
# modificar. male sube +5%: 1.05 * 1.05 = 1.1025.
VARIANT_VISUAL_SCALE_MULTIPLIER = {"male": 1.05, "female": 1.0}


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
    es "left" (macho) o "right" (hembra) -- la dirección de PROVENANCE:
    la orientación REAL importada para este variante (nunca la
    dirección runtime -- ver el docstring del módulo y
    `_invert_runtime_direction()` más abajo para esa distinción). La
    opuesta se deriva acá por espejado determinista. Retorna (manifest,
    reports) -- reports solo para logging en main()."""
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
    #
    # Inversión de dirección RUNTIME (pasada de pulido final, pedido de
    # producto explícito -- ver DEC-091 en docs/DECISION_LOG.md): el
    # owner pidió que TODO lo que hoy se ve corriendo con
    # `Direction::kRight` pase a verse con `Direction::kLeft`, y
    # viceversa, para las DOS variantes de Frin (macho y hembra) y para
    # TODO contenido direccional (pose base sentada/acostada,
    # sit_to_lie, lie_to_sit, howl, tail_greet).
    #
    # Esto es una inversión de SEMÁNTICA DE RUNTIME, no una
    # reorganización de arte fuente: las carpetas de import
    # (`animations/<anim>/{left,right}/frames/`) siguen significando
    # exactamente lo que significaban -- registran qué orientación
    # exportó el owner realmente ("left"/"right" en disco es
    # PROVENANCE, nunca se renombra ni se mueve solo para que el nombre
    # coincida con el nuevo significado runtime). Lo único que cambia es
    # QUÉ carpeta alimenta el slot runtime kRight vs. el slot runtime
    # kLeft del pack compilado.
    #
    # `_invert_runtime_direction()` es el ÚNICO lugar donde se aplica
    # esa inversión -- se intercala DENTRO de `entries_for()`, así que
    # `action()`/`base_pose()` (los únicos llamadores) siguen pidiendo
    # "dame el contenido para el slot runtime right/left" exactamente
    # como antes, sin ningún cambio en ellos ni duplicación de la
    # inversión por cada animación/acción. Ver
    # tests/test_asset_pipeline.py's FrinRuntimeDirectionInversionTest
    # para la prueba de que esto efectivamente intercambia right<->left
    # para las dos variantes, en base y en cada acción.
    def _invert_runtime_direction(runtime_direction: str) -> str:
        return "left" if runtime_direction == "right" else "right"

    def entries_for(anim_id: str, runtime_direction: str) -> list[dict]:
        source_direction = _invert_runtime_direction(runtime_direction)
        return frame_entries[f"{anim_id}_real"] if source_direction == canonical_direction else frame_entries[f"{anim_id}_derived"]

    def action(
        anim_id: str,
        weight: float,
        target_state_id: str,
        *,
        first_frame_is_state_base: str | None = None,
        last_frame_is_state_base: str | None = None,
        match_aspect_to_stable_poses: bool = False,
        match_color_to_stable_poses: bool = False,
        stable_pose_tail_frames: int = 1,
        terminal_rigid_translation: "dict[str, tuple[int, float, float]] | None" = None,
    ) -> dict:
        """IDENTIDAD SEMÁNTICA DE POSE (DEC-099). Cada acción declara
        qué pose ESTABLE representa cada una de sus dos puntas, y el
        compilador compila esa punta desde el ARCHIVO de esa base (ver
        `first_frame_is_state_base` en tools/compile_pet_pack.py). No es
        una medición ni una corrección: es una afirmación de contenido,
        y el frame compilado sale idéntico byte a byte al de la base.

        REEMPLAZA los dos mecanismos anteriores, ya eliminados:
        `align_endpoint_to_target_base` (DEC-092/095/098, que anclaba y
        escalaba por el último frame) y `anchor_start_to_source_base`
        (DEC-097, que anclaba por el primero). Los dos intentaban que
        una punta "casi" coincidiera con una base; ninguno podía llegar
        a exacto, porque el resampleo entero deja su propio residual.

        Para `lie_to_sit` esto además cierra la saga de sus dos puntas:
        ya no hay que ELEGIR cuál anclar (DEC-097 eligió el arranque y
        dejó ~26px de residual al final). Ahora las DOS son exactas, y
        lo que el export no cierra queda absorbido DENTRO del clip,
        donde el lobo ya está en movimiento, en vez de en el instante
        en que queda quieto. El residual sigue siendo deuda de
        CONTENIDO y se mide en el informe -- no se disimula moviendo el
        sprite durante el clip (eso se probó y QA lo rechazó)."""
        manifest_action = {
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
        # Solo se emiten las claves realmente declaradas -- una acción
        # sin puntas estables no lleva el campo en el manifest.
        if first_frame_is_state_base is not None:
            manifest_action["first_frame_is_state_base"] = first_frame_is_state_base
        if last_frame_is_state_base is not None:
            manifest_action["last_frame_is_state_base"] = last_frame_is_state_base
        if match_aspect_to_stable_poses:
            manifest_action["match_aspect_to_stable_poses"] = True
        if match_color_to_stable_poses:
            manifest_action["match_color_to_stable_poses"] = True
        if stable_pose_tail_frames != 1:
            manifest_action["stable_pose_tail_frames"] = stable_pose_tail_frames
        if terminal_rigid_translation is not None:
            # Por-DIRECCIÓN RUNTIME, NUNCA por espejo asumido (pasada de
            # corrección posicional -- ver DEC-105): `dx` se invierte de
            # signo entre kRight/kLeft porque el espejado horizontal
            # invierte el eje X, pero se MIDE por separado para cada una
            # de las dos, sobre el pack YA compilado -- nunca se deriva
            # una y se niega la otra por suposición. Ver el informe de
            # este bloque para la verificación (-23.52 vs +23.44 en el
            # macho: casi exactos pero no idénticos, la asimetría real
            # del contenido).
            right = terminal_rigid_translation.get("right")
            if right is not None:
                start_frame, dx, dy = right
                manifest_action["terminal_rigid_translation"] = {
                    "start_frame": start_frame, "dx": dx, "dy": dy}
            left = terminal_rigid_translation.get("left")
            if left is not None:
                start_frame, dx, dy = left
                manifest_action["direction_overrides"][0]["terminal_rigid_translation"] = {
                    "start_frame": start_frame, "dx": dx, "dy": dy}
        return manifest_action

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
        "visual_scale": VISUAL_SCALE * VARIANT_VISUAL_SCALE_MULTIPLIER[variant],
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
                "ambient_actions": [
                    action("sit_to_lie", 1.0, "lying",
                           first_frame_is_state_base="seated", last_frame_is_state_base="lying")
                ],
                "hover_uses_ambient_actions": False,  # el owner no definió hover para Frin todavía
                "hover_actions": [],
                "click_actions": [
                    action("howl", SEATED_CLICK_WEIGHTS["howl"], "seated",
                           first_frame_is_state_base="seated", last_frame_is_state_base="seated",
                           match_aspect_to_stable_poses=True),
                    action("tail_greet", SEATED_CLICK_WEIGHTS["tail_greet"], "seated",
                           first_frame_is_state_base="seated", last_frame_is_state_base="seated",
                           match_color_to_stable_poses=TAIL_GREET_COLOR_MATCH[variant]),
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
                "click_actions": [
                    action("lie_to_sit", 1.0, "seated",
                           first_frame_is_state_base="lying", last_frame_is_state_base="seated",
                           match_aspect_to_stable_poses=True,
                           terminal_rigid_translation=LIE_TO_SIT_TERMINAL_TRANSLATION[variant])
                ],
            },
        ],
    }
    return manifest, reports


def _finalize_and_compile(variant: str, manifest: dict, canonical_direction: str) -> None:
    variant_root = os.path.join(FRIN_ROOT, variant)
    manifest_path = os.path.join(variant_root, "pack_manifest.json")

    content_plan = compile_pet_pack._build_content_plan(manifest, variant_root)
    _, working_width, working_height, _, _ = content_plan.normalization[
        f"state[{manifest['states'][0]['id']}].base_animation"]
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
