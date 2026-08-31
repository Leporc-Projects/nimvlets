#!/usr/bin/env python3
"""Lector del formato ".nvpack" ("NVPACK2") que compila
`tools/compile_pet_pack.py` — el lado de LECTURA del mismo contrato
binario que `src/content/PetPackLoader.cpp` implementa en C++ (ver
docs/ANIMATION_RUNTIME.md para el layout exacto).

Por qué existe (Block 05, pasada de estabilización): hasta acá el
pipeline solo se podía verificar por su ENTRADA (los PNG fuente) o
mirando la app corriendo. Varias propiedades que sí importan de verdad
son propiedades del ARTEFACTO COMPILADO — en particular el invariante
de continuidad de una transición que cambia de estado (ver
docs/DECISION_LOG.md DEC-087):

    último frame mostrado de la transición
        ==
    primer frame mostrado de la pose base del estado destino

Eso no se puede afirmar desde el manifest ni desde los PNG: depende de
cómo el compilador terminó colocando cada frame en el canvas de trabajo
compartido. Con este lector, un test puede comparar exactamente los
bytes que el runtime va a mostrar.

No decodifica PNG ni valida nada: solo parsea. Sin dependencias de
terceros.

Uso como script (inspección rápida, sin volcar pixeles):
    python3 tools/read_pet_pack.py assets/dev/frin_male_pack.nvpack
"""

from __future__ import annotations

import struct
import sys

MAGIC = b"NVPACK2\0"

_KIND_NAMES = {0: "static", 1: "loop", 2: "one_shot"}
_DIRECTION_NAMES = {0: "right", 1: "left"}


class PackReadError(Exception):
    pass


class _Reader:
    def __init__(self, data: bytes) -> None:
        self._data = data
        self._offset = 0

    def raw(self, count: int) -> bytes:
        if self._offset + count > len(self._data):
            raise PackReadError(f"truncated pack: wanted {count} bytes at offset {self._offset}")
        chunk = self._data[self._offset : self._offset + count]
        self._offset += count
        return chunk

    def u8(self) -> int:
        return self.raw(1)[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.raw(4))[0]

    def f64(self) -> float:
        return struct.unpack("<d", self.raw(8))[0]

    def string(self) -> str:
        return self.raw(self.u32()).decode("utf-8")


def _read_frame(reader: _Reader) -> dict:
    width = reader.u32()
    height = reader.u32()
    anchor_x = reader.f64()
    anchor_y = reader.f64()
    duration_ms = reader.f64()
    return {
        "width": width,
        "height": height,
        "anchor_x": anchor_x,
        "anchor_y": anchor_y,
        "duration_ms": duration_ms,
        "pixels": reader.raw(width * height * 4),
    }


def _read_animation(reader: _Reader) -> dict:
    animation = {
        "id": reader.string(),
        "kind": _KIND_NAMES.get(reader.u8(), "?"),
        "fps": reader.f64(),
        "returns_to_idle": bool(reader.u8()),
    }
    animation["frames"] = [_read_frame(reader) for _ in range(reader.u32())]
    return animation


def _read_direction_overrides(reader: _Reader) -> list[dict]:
    overrides = []
    for _ in range(reader.u32()):
        direction = _DIRECTION_NAMES.get(reader.u8(), "?")
        overrides.append({"direction": direction, "animation": _read_animation(reader)})
    return overrides


def _read_weighted_actions(reader: _Reader) -> list[dict]:
    actions = []
    for _ in range(reader.u32()):
        action = {"id": reader.string(), "weight": reader.f64(), "target_state_id": reader.string()}
        action["animation"] = _read_animation(reader)
        action["direction_overrides"] = _read_direction_overrides(reader)
        actions.append(action)
    return actions


def read_pack_header(path: str) -> dict:
    """Solo la cabecera de identidad/procedencia de un "NVPACK2", sin
    decodificar ni un frame — id, display_name, variant_group,
    canvas, alpha_hit_threshold, visual_scale, content_version.

    Existe (Block 09A, pasada de endurecimiento del gate de contenido)
    para que tools/compile_pet_catalog.py pueda verificar que el pack de
    un starter de producción REALMENTE pertenece a esa identidad de
    catálogo sin leer los ~76 MB de píxeles del pack completo. El layout
    de la cabecera es idéntico al que parsea `read_pack()` y
    `src/content/PetPackLoader.cpp`."""
    with open(path, "rb") as f:
        # La cabecera son unos pocos cientos de bytes; 4 KiB sobra y no
        # depende de cuántos estados/frames tenga el pack.
        head = f.read(4096)
    reader = _Reader(head)

    magic = reader.raw(len(MAGIC))
    if magic != MAGIC:
        raise PackReadError(f"{path}: not an NVPACK2 file (magic was {magic!r})")

    return {
        "id": reader.string(),
        "display_name": reader.string(),
        "variant_group": reader.string(),
        "canvas_width": reader.u32(),
        "canvas_height": reader.u32(),
        "alpha_hit_threshold": reader.u8(),
        "visual_scale": reader.f64(),
        "content_version": reader.string(),
    }


def read_pack(path: str) -> dict:
    with open(path, "rb") as f:
        reader = _Reader(f.read())

    magic = reader.raw(len(MAGIC))
    if magic != MAGIC:
        raise PackReadError(f"{path}: not an NVPACK2 file (magic was {magic!r})")

    pack = {
        "id": reader.string(),
        "display_name": reader.string(),
        "variant_group": reader.string(),
        "canvas_width": reader.u32(),
        "canvas_height": reader.u32(),
        "alpha_hit_threshold": reader.u8(),
        "visual_scale": reader.f64(),
        "content_version": reader.string(),
        "states": [],
    }
    for _ in range(reader.u32()):
        state = {"id": reader.string()}
        state["base_animation"] = _read_animation(reader)
        state["base_animation_direction_overrides"] = _read_direction_overrides(reader)
        state["ambient_interval_seconds"] = reader.f64()
        state["ambient_actions"] = _read_weighted_actions(reader)
        state["hover_uses_ambient_actions"] = bool(reader.u8())
        state["hover_actions"] = _read_weighted_actions(reader)
        state["click_actions"] = _read_weighted_actions(reader)
        pack["states"].append(state)
    return pack


def resolve_animation(animation: dict, direction_overrides: list[dict], direction: str) -> dict:
    """La MISMA resolución que hace `content::ResolveAnimation()` en el
    runtime: si hay un override para `direction`, se muestra ese; si no,
    la animación canónica. Reimplementarlo mal acá haría que un test
    comparara frames que el runtime nunca muestra juntos, así que esta
    función es el único lugar donde vive esa regla del lado Python."""
    for override in direction_overrides:
        if override["direction"] == direction:
            return override["animation"]
    return animation


def find_state(pack: dict, state_id: str) -> dict:
    for state in pack["states"]:
        if state["id"] == state_id:
            return state
    raise PackReadError(f"state '{state_id}' not found in pack '{pack['id']}'")


def find_action(state: dict, trigger: str, action_id: str) -> dict:
    for action in state[trigger]:
        if action["id"] == action_id:
            return action
    raise PackReadError(f"action '{action_id}' not found in state '{state['id']}'.{trigger}")


def content_bbox(frame: dict, alpha_threshold: int = 0) -> tuple[int, int, int, int] | None:
    """Bounding box inclusivo de los pixeles con alpha > `alpha_threshold`."""
    width, height, pixels = frame["width"], frame["height"], frame["pixels"]
    min_x = min_y = None
    max_x = max_y = -1
    for y in range(height):
        row = y * width * 4
        for x in range(width):
            if pixels[row + x * 4 + 3] > alpha_threshold:
                if min_x is None or x < min_x:
                    min_x = x
                if min_y is None or y < min_y:
                    min_y = y
                if x > max_x:
                    max_x = x
                if y > max_y:
                    max_y = y
    if min_x is None:
        return None
    return min_x, min_y, max_x, max_y


def content_centre(frame: dict) -> tuple[float, float] | None:
    bbox = content_bbox(frame)
    if bbox is None:
        return None
    min_x, min_y, max_x, max_y = bbox
    return ((min_x + max_x + 1) / 2.0, (min_y + max_y + 1) / 2.0)


def content_alpha_centroid(frame: dict) -> tuple[float, float] | None:
    """Centroide ("centro de masa") ponderado por alpha de un frame --
    a diferencia de `content_centre()` (centro geométrico del bounding
    box, decidido por los DOS pixeles más extremos), este integra TODOS
    los pixeles con su alpha como peso.

    Es la misma noción que `prep_dev_sprite.alpha_weighted_centroid()`
    (Block 05, pasada de pulido final -- ver docs/DECISION_LOG.md
    DEC-093), reimplementada acá en vez de importada para que este
    módulo se mantenga sin dependencias de terceros NI de otras partes
    del pipeline -- ver el docstring del módulo. Ambas deben coincidir
    exactamente para el mismo buffer de pixeles; si alguna vez
    divergen, es un bug en una de las dos, no una discrepancia de
    diseño esperada.

    Devuelve None para un frame completamente transparente."""
    width, height, pixels = frame["width"], frame["height"], frame["pixels"]
    total = 0
    sum_x = 0
    sum_y = 0
    for y in range(height):
        row = y * width * 4
        for x in range(width):
            a = pixels[row + x * 4 + 3]
            if a:
                total += a
                sum_x += x * a
                sum_y += y * a
    if total == 0:
        return None
    return sum_x / total, sum_y / total


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    pack = read_pack(sys.argv[1])
    print(
        f"pet '{pack['id']}' ({pack['display_name']}) variant='{pack['variant_group']}' "
        f"canvas={pack['canvas_width']}x{pack['canvas_height']} visual_scale={pack['visual_scale']} "
        f"alpha_hit_threshold={pack['alpha_hit_threshold']} content_version='{pack['content_version']}'"
    )
    for state in pack["states"]:
        print(f"  state '{state['id']}' ambient_interval={state['ambient_interval_seconds']}s")
        base = state["base_animation"]
        print(f"    base_animation '{base['id']}' {base['kind']} {len(base['frames'])}f "
              f"{base['frames'][0]['width']}x{base['frames'][0]['height']}"
              f" (+{len(state['base_animation_direction_overrides'])} direction override(s))")
        for trigger in ("ambient_actions", "hover_actions", "click_actions"):
            for action in state[trigger]:
                animation = action["animation"]
                print(f"    {trigger[:-8]:<7} '{action['id']}' weight={action['weight']} "
                      f"-> '{action['target_state_id']}' {len(animation['frames'])}f")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
