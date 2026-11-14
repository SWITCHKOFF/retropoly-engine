# SPDX-License-Identifier: MIT
# RetroPolyEngine — src/hal/n64/rsp_tnl.S
#
# RSP T&L overlay (Спринт 43–44): transform 3 vertices одним 4×4 матричным
# умножением на векторном блоке RSP (Q16.16, fixed-point). Микрокод — RSPQ
# overlay (libdragon, pinned commit): код и данные грузятся/выгружаются
# очередью, состояние (буферы) сохраняется между командами.
#
# Контракт CPU↔RSP (n64_render_bridge.h / rsp_tnl.cpp):
#   - слот = { int16_t i[16]; uint16_t f[16]; } в формате «hi/lo»
#     (как examples/rspqdemo/vec_slot_t): две половины по 8 компонент;
#   - input:  комп. 0..3 = v0.xyzw, 4..7 = v1.xyzw, 8..11 = v2.xyzw,
#     w = 1.0; 12..15 = 0;
#   - matrix: 16 компонент GL column-major (4 группы по 4) — передаётся
#     В САМОЙ КОМАНДЕ (без гонки за общий DMEM-буфер между кадрами);
#   - output: комп. 0..3 = clip.v0.xyzw, 4..7 = v1, 8..11 = v2.
#
# Микрокод — производная от examples/rspqdemo/rsp_vec.S (libdragon, MIT):
# последовательность vmudl/vmadm/vmadn/vmadh для 32-bit Q16.16 MAC.
# Ошибки: RSP-crash с snapshot (rspq debug); CPU-эталон (rsp_tnl.cpp)
# сверяется в ROM self-test и host-тестах. Контракт вызывающего: bounded
# batch (≤512 слотов) + rspq_wait.
    #include <rsp_queue.inc>

    .set noreorder
    .set at

    .data

RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand TnlCmd_Transform, 12
RSPQ_EndOverlayHeader

# Состояние overlay: матрица (копия из команды) + входной/выходной слоты.
RSPQ_BeginSavedState
    .align 4
TNL_MATRIX: .ds.b 64
TNL_IN:     .ds.b 64
TNL_OUT:    .ds.b 64
RSPQ_EndSavedState

    .text

# ---------------------------------------------------------------------------
# TNL_MATRIX_VEC: out[out_off..] = M * in[in_off..], 8 компонент (2 vec4).
# Тело — examples/rspqdemo/rsp_vec.S (MIT); адреса DMEM — константы
# оверлея, поэтому matrix/vec/out вычисляются из TNL_* напрямую.
# ---------------------------------------------------------------------------
    .macro TNL_MATRIX_VEC in_off, out_off
    #define trans_mtx t0
    #define trans_vec t1
    #define trans_out t2
    #define m0i $v01
    #define m0f $v02
    #define m1i $v03
    #define m1f $v04
    #define m2i $v05
    #define m2f $v06
    #define m3i $v07
    #define m3f $v08
    #define v01i $v09
    #define v01f $v10
    #define o01i $v13
    #define o01f $v14
    #define vtemp $v17

    addiu trans_mtx, zero, %lo(TNL_MATRIX)
    addiu trans_vec, zero, %lo(TNL_IN) + \in_off
    addiu trans_out, zero, %lo(TNL_OUT) + \out_off

    # Матрица: 4 группы компонент (столбцы GL), каждая повторена дважды.
    ldv m0i, 0x0, 0x00, trans_mtx
    ldv m0i, 0x8, 0x00, trans_mtx
    ldv m0f, 0x0, 0x10, trans_mtx
    ldv m0f, 0x8, 0x10, trans_mtx
    ldv m1i, 0x0, 0x08, trans_mtx
    ldv m1i, 0x8, 0x08, trans_mtx
    ldv m1f, 0x0, 0x18, trans_mtx
    ldv m1f, 0x8, 0x18, trans_mtx
    ldv m2i, 0x0, 0x20, trans_mtx
    ldv m2i, 0x8, 0x20, trans_mtx
    ldv m2f, 0x0, 0x30, trans_mtx
    ldv m2f, 0x8, 0x30, trans_mtx
    ldv m3i, 0x0, 0x28, trans_mtx
    ldv m3i, 0x8, 0x28, trans_mtx
    ldv m3f, 0x0, 0x38, trans_mtx
    ldv m3f, 0x8, 0x38, trans_mtx

    # Два vec4: компоненты 0..7 (i[0..7] и f[0..7]).
    lqv v01i, 0x0, 0x00, trans_vec
    lqv v01f, 0x0, 0x10, trans_vec

    # clip.x = dot(row0, v)
    vmudl vtemp, m0f, v01f, e(0h)
    vmadm vtemp, m0i, v01f, e(0h)
    vmadn vtemp, m0f, v01i, e(0h)
    vmadh vtemp, m0i, v01i, e(0h)

    # clip.y = dot(row1, v)
    vmadl vtemp, m1f, v01f, e(1h)
    vmadm vtemp, m1i, v01f, e(1h)
    vmadn vtemp, m1f, v01i, e(1h)
    vmadh vtemp, m1i, v01i, e(1h)

    # clip.z = dot(row2, v)
    vmadl vtemp, m2f, v01f, e(2h)
    vmadm vtemp, m2i, v01f, e(2h)
    vmadn vtemp, m2f, v01i, e(2h)
    vmadh vtemp, m2i, v01i, e(2h)

    # clip.w = dot(row3, v)
    vmadl vtemp, m3f, v01f, e(3h)
    vmadm vtemp, m3i, v01f, e(3h)
    vmadn o01f, m3f, v01i, e(3h)
    vmadh o01i, m3i, v01i, e(3h)

    sqv o01i, 0x0, 0x00, trans_out
    sqv o01f, 0x0, 0x10, trans_out

    #undef trans_mtx
    #undef trans_vec
    #undef trans_out
    #undef m0i
    #undef m0f
    #undef m1i
    #undef m1f
    #undef m2i
    #undef m2f
    #undef m3i
    #undef m3f
    #undef v01i
    #undef v01f
    #undef o01i
    #undef o01f
    #undef vtemp
    .endm

# ---------------------------------------------------------------------------
# Transform: [ID|in_addr][matrix_addr][out_addr] (12-byte команда).
# Матрица — отдельный RDRAM-буфер на слот (без гонки за DMEM).
# ---------------------------------------------------------------------------
TnlCmd_Transform:
    move s5, ra
    move s7, a1
    move s8, a2
    li t0, 0x00FFFFFF
    and s6, a0, t0

    # matrix → DMEM saved state
    move s0, s7
    addiu s4, zero, %lo(TNL_MATRIX)
    li t1, 0
    jal DMAIn
    li t0, DMA_SIZE(64, 1)

    move s0, s6
    addiu s4, zero, %lo(TNL_IN)
    li t1, 0
    jal DMAIn
    li t0, DMA_SIZE(64, 1)

    # ---- slot 0: components 0..7 (v0.xyzw, v1.xyzw) ----------------------
    TNL_MATRIX_VEC 0, 0
    # ---- slot 1: components 8..15 (v2.xyzw, pad) -------------------------
    TNL_MATRIX_VEC 32, 32

    move s0, s8
    addiu s4, zero, %lo(TNL_OUT)
    li t1, 0
    jal DMAOut
    li t0, DMA_SIZE(64, 1)
    jr s5
    nop

