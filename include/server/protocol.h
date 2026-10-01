#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

// 线协议。**这些结构原本长在 include/fsm/iControlBlock.h 里**，与 FSM 的 IO 抽象、
// 具体的 asio 控制块混在一个头文件 —— 想换传输层就得连 FSM 的头一起改。
// 现在协议归协议：FSM 只认 include/fsm/iControlBlock.h 的三个方法。
//
// 帧格式（与既有协议逐字节一致，老客户端不需要改）：
//   [4B 大端 total_length（含自身）][2B 大端 command][4B 大端 uuid][payload...]
// 最小帧长 10。total_length 由 4 字节大端承载，所以成帧器上限决定了单帧上限。

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace proto {

constexpr std::size_t kHeaderSize = 10;

// 线协议版本（2026-10-01 引入，随 HELLO 命令下发）。只在**破坏性变更**（改既有
// 帧语义/删命令）时手动加一——加命令/加字段不 bump（指纹会自动暴露变化，
// 见 output_json 的 version_fingerprint：姿势对齐 core_api.h 的 verify_plugin_abi）。
constexpr std::uint16_t kProtocolVersion = 1;

enum class Command : std::uint16_t {
    INVALID = 0,

    // ── 操作命令（客户端 → 服务端）──
    SELECT_SKILL = 1,    // payload: int32 robot, int32 action_type, int32 index
    USE_MEDICINE = 2,    // 同上
    CHOOSE_PET = 3,      // 同上
    SEND_EMOJI = 4,      // 未使用（保留兼容）
    INIT_BATTLE = 5,     // 二进制阵容：每只精灵 13 个 int32（见 pet_factory.cpp）
    INIT_BATTLE_JSON = 6,  // 本轮新增：JSON 阵容。给 Web/Unity 前端用，免去手拼二进制

    // ── 状态查询 ──
    SYNC_STATE = 10,     // 要一份权威快照（复用 BattleContext::getStateJson）
    HEARTBEAT = 11,

    // ── 调试命令 ──
    DEBUG_STEP = 20,        // 开单步：此后 FSM 每执行一个时点就停
    DEBUG_CONTINUE = 21,    // 关单步：连续跑到需要输入/结束
    DEBUG_BREAKPOINT = 22,  // toggle 断点：payload int32 state_id
    DEBUG_FULLSTATE = 23,   // 完整状态转储（含效果表）
    DEBUG_MUTATE = 24,      // 调试手术（二期，2026-09-26）：payload JSON 白名单操作集，
                            // 仅在 FSM 泊车（等输入）时执行；回帧带逐条操作回执

    // ── 服务端 → 客户端（本轮新增）──
    // 事件带：本次推进（一次 run）内每个时点一条采样，按 seq 有序。
    TAPE = 30,
    // 该你了：payload 是 JSON，含 player / legal 合法动作集 / 快照摘要。
    INPUT_REQUIRED = 31,
    // 单独请求合法动作集时回这个（INPUT_REQUIRED 里也带一份）。
    LEGAL_ACTIONS = 32,
    // 错误文本（FSM 原本把裸文本直接写进流，会破坏长度前缀成帧，这里补上帧）。
    ERROR = 33,
    // 入座/房间信息：match_id、seat、座位数、solo 还是 pvp。
    ROOM_INFO = 34,
    // 战斗结束：payload 是 JSON（winner + 最终快照）。
    BATTLE_OVER = 35,
    // 版本握手（2026-10-01）：C→S payload 忽略；S→C 回 JSON 指纹
    // {"protoVersion","registryHash","stateSchemaHash"}。网关/前端连接即核对，
    // 内核加了字段/新枚举而显示面没跟上时**响亮报警**而不是静默漏显。
    HELLO = 36,
};

inline const char* command_name(Command cmd) {
    switch (cmd) {
        case Command::INVALID: return "INVALID";
        case Command::SELECT_SKILL: return "SELECT_SKILL";
        case Command::USE_MEDICINE: return "USE_MEDICINE";
        case Command::CHOOSE_PET: return "CHOOSE_PET";
        case Command::INIT_BATTLE: return "INIT_BATTLE";
        case Command::INIT_BATTLE_JSON: return "INIT_BATTLE_JSON";
        case Command::SYNC_STATE: return "SYNC_STATE";
        case Command::HEARTBEAT: return "HEARTBEAT";
        case Command::DEBUG_STEP: return "DEBUG_STEP";
        case Command::DEBUG_CONTINUE: return "DEBUG_CONTINUE";
        case Command::DEBUG_BREAKPOINT: return "DEBUG_BREAKPOINT";
        case Command::DEBUG_FULLSTATE: return "DEBUG_FULLSTATE";
        case Command::DEBUG_MUTATE: return "DEBUG_MUTATE";
        case Command::TAPE: return "TAPE";
        case Command::INPUT_REQUIRED: return "INPUT_REQUIRED";
        case Command::LEGAL_ACTIONS: return "LEGAL_ACTIONS";
        case Command::ERROR: return "ERROR";
        case Command::ROOM_INFO: return "ROOM_INFO";
        case Command::BATTLE_OVER: return "BATTLE_OVER";
        case Command::HELLO: return "HELLO";
        default: return "UNKNOWN";
    }
}

struct Header {
    std::uint32_t total_length = 0;
    Command command = Command::INVALID;
    std::uint32_t uuid = 0;
};

// ── 大端编解码 ──
// 手写而不用 htonl：这样协议头不依赖 <arpa/inet.h>，非 POSIX 平台上也能编译。
// 线格式必须保持大端 —— 既有客户端（training_cli）按大端读写。

inline void write_u16_be(char* p, std::uint16_t v) {
    p[0] = static_cast<char>((v >> 8) & 0xff);
    p[1] = static_cast<char>(v & 0xff);
}

inline void write_u32_be(char* p, std::uint32_t v) {
    p[0] = static_cast<char>((v >> 24) & 0xff);
    p[1] = static_cast<char>((v >> 16) & 0xff);
    p[2] = static_cast<char>((v >> 8) & 0xff);
    p[3] = static_cast<char>(v & 0xff);
}

inline std::uint16_t read_u16_be(const char* p) {
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(p[0]) << 8) |
                                      static_cast<std::uint8_t>(p[1]));
}

inline std::uint32_t read_u32_be(const char* p) {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(p[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(p[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(p[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(p[3]));
}

// 组一帧。payload 可以是二进制，也可以是 JSON 文本。
inline std::string build_frame(Command cmd, std::uint32_t uuid, const std::string& payload) {
    std::string frame(kHeaderSize, '\0');
    const std::uint32_t total = static_cast<std::uint32_t>(kHeaderSize + payload.size());
    write_u32_be(&frame[0], total);
    write_u16_be(&frame[4], static_cast<std::uint16_t>(cmd));
    write_u32_be(&frame[6], uuid);
    frame += payload;
    return frame;
}

// 解析帧头。frame_len 必须 >= kHeaderSize。
inline bool parse_header(const char* frame, std::size_t frame_len, Header& out) {
    if (frame == nullptr || frame_len < kHeaderSize) {
        return false;
    }
    out.total_length = read_u32_be(frame);
    out.command = static_cast<Command>(read_u16_be(frame + 4));
    out.uuid = read_u32_be(frame + 6);
    return true;
}

// 操作载荷：三个 int32（robot / action_type / index）。空位一律 0。
struct ActionPayload {
    int robot = -1;
    int action_type = -1;
    int index = -1;
};

inline std::string build_action_payload(int robot, int action_type, int index) {
    std::string out(3 * sizeof(std::int32_t), '\0');
    write_u32_be(&out[0], static_cast<std::uint32_t>(robot));
    write_u32_be(&out[4], static_cast<std::uint32_t>(action_type));
    write_u32_be(&out[8], static_cast<std::uint32_t>(index));
    return out;
}

// 与 FSM 交接用的 4 int 缓冲（BattleContext::m_buffer）。
// FSM 读 buf[0]=robot buf[1]=action_type buf[2]=index，buf[3] 未用。
//
// ⚠️⚠️ 这里必须是**本机字节序**，不能写成大端。
//    FSM 用 `memcpy(buf, m_buffer.data(), 4 * sizeof(int))` 直接把它当成 `int[4]` 读
//    （见 battleFsm.cpp 的 handle_OperationChooseSkillMedicament / handle_ChooseAfterDeath）。
//    线格式统一大端，大端→本机的转换就在这一行完成。
//    写成大端的实际后果：小端机上 action_type=1 会变成 16777216，
//    operation() 落进 default 分支什么都不做，动作被判"rejected, please resubmit"。
inline std::string build_fsm_input(int robot, int action_type, int index) {
    std::string out(4 * sizeof(std::int32_t), '\0');
    const std::int32_t native_input[4] = {robot, action_type, index, 0};
    std::memcpy(&out[0], native_input, sizeof(native_input));
    return out;
}

inline ActionPayload decode_action_payload(const char* data, std::size_t len) {
    ActionPayload out;
    if (data == nullptr || len < 3 * sizeof(std::int32_t)) {
        return out;
    }
    out.robot = static_cast<int>(read_u32_be(data));
    out.action_type = static_cast<int>(read_u32_be(data + 4));
    out.index = static_cast<int>(read_u32_be(data + 8));
    return out;
}

// 动作类型（与 BattleFsm::ActionType 的整数值一致，与客户端约定一致）
enum class ActionType : int {
    CHOOSE_PET = 0,
    SELECT_SKILL = 1,
    USE_MEDICINE = 2,
    NONE = 3,   // 空操作（超时/什么都不做）：回合类与异常后续照常
};

inline const char* action_type_name(int t) {
    switch (t) {
        case 0: return "choose_pet";
        case 1: return "skill";
        case 2: return "medicine";
        default: return "unknown";
    }
}

}  // namespace proto

#endif  // SERVER_PROTOCOL_H
