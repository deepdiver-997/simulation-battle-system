"""sim_server 线协议编解码 —— 与 include/server/protocol.h 逐字节一致。

帧格式：[4B 大端 total_length（含自身）][2B 大端 command][4B 大端 uuid][payload...]
动作 payload 是 3 个大端 int32（robot, action_type, index）；
action_type 沿用 FSM 枚举：0=换宠 1=技能 2=药剂。
"""

import struct

HEADER_SIZE = 10
MAX_FRAME = 1 << 20  # 与服务端 Session 成帧上限一致

# ── 客户端 → 服务端 ──
C_SELECT_SKILL = 1
C_USE_MEDICINE = 2
C_CHOOSE_PET = 3
C_INIT_BATTLE = 5
C_INIT_BATTLE_JSON = 6
C_SYNC_STATE = 10
C_HEARTBEAT = 11
C_DEBUG_STEP = 20
C_DEBUG_CONTINUE = 21
C_DEBUG_BREAKPOINT = 22
C_DEBUG_FULLSTATE = 23
C_DEBUG_MUTATE = 24
C_LEGAL_ACTIONS = 32

# ── 服务端 → 客户端 ──
S_TAPE = 30
S_INPUT_REQUIRED = 31
S_ERROR = 33
S_ROOM_INFO = 34
S_BATTLE_OVER = 35

C_HELLO = 36              # 版本握手：回引擎指纹 JSON（2026-10-01，protocol.h HELLO）

# 双向命令（服务端原样回命令号）
BIDIR_SYNC_STATE = 10
BIDIR_HELLO = 36
BIDIR_HEARTBEAT = 11
BIDIR_DEBUG_FULLSTATE = 23
BIDIR_LEGAL_ACTIONS = 32

CMD_NAMES = {
    C_SELECT_SKILL: "select_skill",
    C_USE_MEDICINE: "use_medicine",
    C_CHOOSE_PET: "choose_pet",
    C_INIT_BATTLE: "init_battle",
    C_HELLO: "hello",
    C_INIT_BATTLE_JSON: "init_battle_json",
    C_SYNC_STATE: "state",
    C_HEARTBEAT: "heartbeat",
    C_DEBUG_STEP: "debug_step",
    C_DEBUG_CONTINUE: "debug_continue",
    C_DEBUG_BREAKPOINT: "debug_breakpoint",
    C_DEBUG_FULLSTATE: "fullstate",
    C_DEBUG_MUTATE: "mutate_result",
    C_LEGAL_ACTIONS: "legal",
    S_TAPE: "tape",
    S_INPUT_REQUIRED: "input_required",
    S_ERROR: "error",
    S_ROOM_INFO: "room_info",
    S_BATTLE_OVER: "battle_over",
}


def build_frame(cmd: int, uuid: int = 0, payload: bytes = b"") -> bytes:
    header = struct.pack(">IHI", HEADER_SIZE + len(payload), cmd, uuid)
    return header + payload


def build_action(robot: int, action_type: int, index: int) -> bytes:
    # 服务端 dispatch 对 SELECT_SKILL/USE_MEDICINE/CHOOSE_PET 一视同仁，
    # 真正分流的 payload 里的 action_type；这里统一用 SELECT_SKILL 命令号。
    return struct.pack(">III", robot, action_type, index)


def build_i32(value: int) -> bytes:
    return struct.pack(">I", value & 0xFFFFFFFF)


def parse_header(buf: memoryview) -> tuple[int, int, int]:
    total, cmd, uuid = struct.unpack_from(">IHI", buf)
    return total, cmd, uuid
