#ifndef SERVER_BATTLE_INIT_H
#define SERVER_BATTLE_INIT_H

// 阵容解析。
//
// 既有的二进制格式（每只精灵 13 个 int32：pet_id + 5 技能 + 6 base + common_trait_id）
// 由 sim_core 的 decode_battle_create_request 处理，**保留不动**（老客户端继续能用）。
// 这里只加 JSON 形式 —— 让 Web / Unity 前端不必手拼二进制块。
//
// JSON 形式（solo，一次给双方）：
// {
//   "side1": [ {"petId":12,"skills":[10038,10057,10001,10008,10009],
//               "base":[0,0,0,0,0,0],"traitId":0}, ... 共 6 只 ],
//   "side2": [ ... ]
// }
// 便捷写法：某一侧写成**单个对象**而不是数组时，视为"这只精灵重复 6 次"。
// 调机制时（双方同阵容）这一条能把载荷从 12 只压到 2 只。
//
// pvp 单边形式（每个座位提交自己那一边）：
// { "party": [ ... 6 只 ] }   —— 同样支持单对象简写。

#include <array>
#include <string>

#include <entities/pet_factory.h>

namespace server {

// 两侧阵容。solo 用。
bool decode_lineup_json(const std::string& text, BattleCreateRequest& out, std::string& err);

// 单边阵容（6 只）。pvp 用。
bool decode_party_json(const std::string& text, std::array<BattlePetMessage, 6>& out,
                       std::string& err);

}  // namespace server

#endif  // SERVER_BATTLE_INIT_H
