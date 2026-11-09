#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/make_source_assets.py
#
# Детерминированный генератор ИСХОДНЫХ ассетов Спринта 9–10:
#   assets/source/cube.gltf — Blender-канонический куб (24 вершины по граням,
#                             12 треугольников, NORMAL+TEXCOORD_0, embedded buffer);
#   assets/source/texture.png — 256×256 RGBA8 (65536 уникальных цветов —
#                             честный стресс Median Cut до ≤256).
# Детерминированный источник ассета Спринта 11–12 (неделя 21):
#   assets/source/triangle.gltf — одного треугольника (3 вершины, NORMAL+
#                             TEXCOORD_0, embedded buffer); winding CCW «к
#                             камере» по конвенции ядра (экрана A<0, y-вниз).
#
# Запуск: python3 scripts/make_source_assets.py [корень репо]
# Байтовая детерминированность: сортированный JSON, без таймстёмпов.
import base64
import json
import os
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# cube.gltf — Blender-каноническая геометрия (±1 куб, CCW-наружу, right-handed
# glTF Y-up). Вершины дублируются по граням (4 на грань: нормали/UV — плоские).
# ---------------------------------------------------------------------------

# Углы куба (индексы 0..7 как в tests/integration/mesh_loader_test.cpp).
CORNERS = [
    (-1, -1, -1), (1, -1, -1), (-1, 1, -1), (1, 1, -1),
    (-1, -1, 1), (1, -1, 1), (-1, 1, 1), (1, 1, 1),
]

# Грани: квад по порядку CCW-наружу (проверено cross(b−a, c−a)·n > 0),
# нормаль наружу, UV-углы (0,0),(1,0),(1,1),(0,1) по порядку квадов.
FACES = [
    # (quad corner indices, normal)
    ([0, 4, 6, 2], (-1, 0, 0)),   # −X
    ([1, 3, 7, 5], (1, 0, 0)),    # +X
    ([0, 1, 5, 4], (0, -1, 0)),   # −Y
    ([2, 6, 7, 3], (0, 1, 0)),    # +Y
    ([0, 2, 3, 1], (0, 0, -1)),   # −Z
    ([4, 5, 7, 6], (0, 0, 1)),    # +Z
]

UV_CORNERS = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]


def build_cube_buffers():
    positions = []
    normals = []
    texcoords = []
    indices = []
    for quad, normal in FACES:
        face_vertex_base = len(positions)
        for k, corner_index in enumerate(quad):
            positions.append(CORNERS[corner_index])
            normals.append(normal)
            texcoords.append(UV_CORNERS[k])
        # fan: (c0,c1,c2) + (c0,c2,c3)
        indices.extend([
            face_vertex_base + 0, face_vertex_base + 1, face_vertex_base + 2,
            face_vertex_base + 0, face_vertex_base + 2, face_vertex_base + 3,
        ])
    return positions, normals, texcoords, indices


def pack_float32(values):
    return b"".join(struct.pack("<f", float(v)) for v in values)


def pack_uint16(values):
    return b"".join(struct.pack("<H", int(v)) for v in values)


def build_triangle_buffers():
    # Один треугольник в плоскости XY (z=0), glTF right-handed Y-up:
    #   v0 (-1,-1,0) — низ-лево; v1 (1,-1,0) — низ-право; v2 (0,1,0) — верх.
    # Winding: обход v0→v1→v2 — CCW «от наблюдателя со стороны +Z» при
    # screen-projected y-вниз (front-face конвенция ядра рендера, A < 0;
    # см. tests/visual/w0_scene.cpp W0_CASE_VISIBLE — тот же порядок BL→BR→TOP).
    #
    # ЧИСЛО индексных записей: формальный контракт §8.3 требует
    # index_count % 3 == 0 (MeshIndex = 6-байтовая запись-треугольник;
    # правило сохранено дословно из SAMPLE-загрузчика, docs/ASSETS.md §2.1
    # «избыточно, но нормативно»). Ровно 1 треугольник в файле этот гейт
    # не проходит, поэтому источник несёт ТРИ идентичные индексные записи
    # (0,1,2)×3 на общих вершинах: пиксельный и hash-результат кадра
    # идентичен одиночному треугольнику (одинаковые вершины/UV/цвета;
    # вершина Painter's-сортировки — одна и та же). Решение зафиксировано
    # в docs/plans/sprint_11_12.md (неделя 21).
    positions = [(-1, -1, 0), (1, -1, 0), (0, 1, 0)]
    normals = [(0, 0, 1), (0, 0, 1), (0, 0, 1)]
    # UV: покрывают текстуру по низу и верху (v=1 в апексе; u=0.5 в апексе —
    # паттерн texture.png читается целиком, шов по u=0.5 нейтрален).
    texcoords = [(0.0, 0.0), (1.0, 0.0), (0.5, 1.0)]
    indices = [0, 1, 2, 0, 1, 2, 0, 1, 2]
    return positions, normals, texcoords, indices


def make_triangle_gltf(path):
    positions, normals, texcoords, indices = build_triangle_buffers()
    pos_bytes = pack_float32(v for triple in positions for v in triple)
    nrm_bytes = pack_float32(v for triple in normals for v in triple)
    uv_bytes = pack_float32(v for pair in texcoords for v in pair)
    idx_bytes = pack_uint16(indices)

    xs = [c[0] for c in positions]
    ys = [c[1] for c in positions]
    zs = [c[2] for c in positions]
    pos_min = [min(xs), min(ys), min(zs)]
    pos_max = [max(xs), max(ys), max(zs)]

    buffers_blob = idx_bytes + pos_bytes + nrm_bytes + uv_bytes
    b64 = base64.b64encode(buffers_blob).decode("ascii")

    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(idx_bytes),
         "target": 34963},
        {"buffer": 0, "byteOffset": len(idx_bytes), "byteLength": len(pos_bytes),
         "target": 34962},
        {"buffer": 0, "byteOffset": len(idx_bytes) + len(pos_bytes),
         "byteLength": len(nrm_bytes), "target": 34962},
        {"buffer": 0,
         "byteOffset": len(idx_bytes) + len(pos_bytes) + len(nrm_bytes),
         "byteLength": len(uv_bytes), "target": 34962},
    ]
    accessors = [
        {"bufferView": 0, "componentType": 5123, "count": len(indices),
         "type": "SCALAR"},
        {"bufferView": 1, "componentType": 5126, "count": len(positions),
         "type": "VEC3", "min": pos_min, "max": pos_max},
        {"bufferView": 2, "componentType": 5126, "count": len(positions),
         "type": "VEC3"},
        {"bufferView": 3, "componentType": 5126, "count": len(positions),
         "type": "VEC2"},
    ]
    gltf = {
        "asset": {"version": "2.0", "generator": "RetroPolyEngine sprint_11_12"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "Triangle"}],
        "meshes": [{"name": "TriangleMesh", "primitives": [{
            "attributes": {
                "POSITION": 1, "NORMAL": 2, "TEXCOORD_0": 3,
            },
            "indices": 0, "mode": 4,
        }]}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(buffers_blob),
                     "uri": "data:application/octet-stream;base64," + b64}],
    }
    text = json.dumps(gltf, sort_keys=True, indent=2) + "\n"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote", path, len(text), "bytes (json)", len(buffers_blob), "buffer bytes")


def make_cube_gltf(path):
    positions, normals, texcoords, indices = build_cube_buffers()

    pos_bytes = pack_float32(v for triple in positions for v in triple)
    nrm_bytes = pack_float32(v for triple in normals for v in triple)
    uv_bytes = pack_float32(v for pair in texcoords for v in pair)
    idx_bytes = pack_uint16(indices)

    xs = [c[0] for c in positions]
    ys = [c[1] for c in positions]
    zs = [c[2] for c in positions]
    pos_min = [min(xs), min(ys), min(zs)]
    pos_max = [max(xs), max(ys), max(zs)]

    buffers_blob = idx_bytes + pos_bytes + nrm_bytes + uv_bytes
    b64 = base64.b64encode(buffers_blob).decode("ascii")

    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(idx_bytes),
         "target": 34963},
        {"buffer": 0, "byteOffset": len(idx_bytes), "byteLength": len(pos_bytes),
         "target": 34962},
        {"buffer": 0, "byteOffset": len(idx_bytes) + len(pos_bytes),
         "byteLength": len(nrm_bytes), "target": 34962},
        {"buffer": 0, "byteOffset": len(idx_bytes) + len(pos_bytes) + len(nrm_bytes),
         "byteLength": len(uv_bytes), "target": 34962},
    ]
    accessors = [
        {"bufferView": 0, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        {"bufferView": 1, "componentType": 5126, "count": len(positions), "type": "VEC3",
         "min": pos_min, "max": pos_max},
        {"bufferView": 2, "componentType": 5126, "count": len(positions), "type": "VEC3"},
        {"bufferView": 3, "componentType": 5126, "count": len(positions), "type": "VEC2"},
    ]
    gltf = {
        "asset": {"version": "2.0", "generator": "RetroPolyEngine sprint_09_10"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "Cube"}],
        "meshes": [{"name": "CubeMesh", "primitives": [{
            "attributes": {
                "POSITION": 1, "NORMAL": 2, "TEXCOORD_0": 3,
            },
            "indices": 0, "mode": 4,
        }]}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(buffers_blob),
                     "uri": "data:application/octet-stream;base64," + b64}],
    }
    text = json.dumps(gltf, sort_keys=True, indent=2) + "\n"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote", path, len(text), "bytes (json)", len(buffers_blob), "buffer bytes")


def png_chunk(kind, payload):
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def make_texture_png(path, size=256):
    raw = bytearray()
    for y in range(size):
        raw.append(0)  # filter type 0 (None) на каждую строку
        for x in range(size):
            r = x & 0xFF
            g = y & 0xFF
            b = (x ^ y) & 0xFF
            raw.extend((r, g, b, 255))
    idat = zlib.compress(bytes(raw), 9)
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr) +
           png_chunk(b"IDAT", idat) + png_chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
    print("wrote", path, len(png), "bytes")


def main():
    root = (sys.argv[1] if len(sys.argv) > 1
            else os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    src = os.path.join(root, "assets", "source")
    os.makedirs(src, exist_ok=True)
    make_cube_gltf(os.path.join(src, "cube.gltf"))
    make_texture_png(os.path.join(src, "texture.png"), 256)
    make_triangle_gltf(os.path.join(src, "triangle.gltf"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
