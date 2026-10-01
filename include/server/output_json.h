#ifndef SERVER_OUTPUT_JSON_H
#define SERVER_OUTPUT_JSON_H

// 服务端 → 客户端的载荷序列化。
//
// 输出分三条（设计依据见 docs/01-架构与设计/网络层与服务端形状.md）：
//   1. 事件带（TAPE）—— 本次推进每个时点一条采样，前端像放胶片一样播
//   2. 快照（SYNC_STATE / DEBUG_FULLSTATE）—— 复用 BattleContext 既有的 JSON 输出
//   3. 合法动作集（INPUT_REQUIRED 里带）—— 此刻能点什么
//
// 这里只做"把内存里的东西写成 JSON 字符串"，不含任何判定逻辑。
// 手写字符串拼接（与 BattleContext::getStateJson 一致），不引入 JSON 依赖。

#include <server/legal_actions.h>

#include <string>
#include <vector>

class BattleContext;
struct StateSample;

namespace server {

// JSON 字符串转义。战斗里的中文名、技能名都可能进来，必须走它。
std::string json_escape(const std::string& in);

// EventType 的可读名（"EVENT_TAKE_DAMAGE" 等）。前端按名字分发比按数字稳。
const char* event_type_name(int event_type);

// 事件带。samples 为空时返回空串（调用方据此跳过发送）。
std::string tape_to_json(int match_id, const std::vector<StateSample>& samples);

// 合法动作集。
std::string legal_actions_to_json(const LegalActions& legal);

// 该你操作了：把人、合法动作集、以及一份权威快照一起给客户端。
// snapshot_json 传 BattleContext::getStateJson() 的结果（"立刻可渲染"的那份）。
std::string input_required_to_json(int match_id, BattleContext& ctx, int player,
                                   const std::string& snapshot_json);

// 入座/房间信息。
std::string room_info_to_json(int match_id, int seat, int seat_count, bool solo);

// 战斗结束。带一份最终快照，省掉客户端再问一次。
// reason: "normal" = 引擎自己走到 FINISHED；
//         "engine_no_legal_action" = 引擎卡在无法满足的输入要求上，服务端兜底结束
//         （见 docs 里的说明，这是引擎缺口，不是正常结束）。
std::string battle_over_to_json(int match_id, BattleContext& ctx, const std::string& snapshot_json,
                                const std::string& reason);

// ── 注册表导出与版本指纹（2026-10-01）──────────────────────────────────
//
// 动机（显示面覆盖盲区治理）：网关对战斗帧是透明转发，内核加字段/新枚举时旧显示面
// **静默漏显**。解法 = 引擎自描述指纹（HELLO 命令下发），网关/前端连接即核对——
// 姿势对齐插件 ABI（core_api.h verify_plugin_abi：内容自描述、装载即核、响亮报警）。
// 指纹三个分量，两个**自动派生**（内容变→指纹变，不存在"忘了 bump"）：
//   protoVersion     手动，仅破坏性变更加一（protocol.h kProtocolVersion）
//   registryHash     注册表导出全文的 FNV-1a（时点/异常/票类型/已注册效果变化→变）
//   stateSchemaHash  getStateJson 顶层键集的 FNV-1a（快照加/删字段→变）

struct RegistryDump {
    std::string json;      // 完整注册表 JSON（build/registry.json 的文件内容）
    std::size_t moves = 0;
    std::size_t soul = 0;
    std::size_t suit = 0;
};

// 组注册表 JSON（--dump-registry 与指纹共用的唯一实现）。
RegistryDump build_registry_dump();

// HELLO 回帧 payload：{"protoVersion":N,"registryHash":"…","stateSchemaHash":"…"}。
// stateSchemaHash 探针失败时为空串（消费方跳过该维比对，不阻塞握手）。
std::string version_fingerprint_json();

}  // namespace server

#endif  // SERVER_OUTPUT_JSON_H
