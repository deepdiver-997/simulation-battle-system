#ifndef FUTURE_LINK_H
#define FUTURE_LINK_H

// ═══════════════════════════════════════════════════════════════════════════
// 斯布林蒂 4600 —— **未来链接 共享层**（lingchao_layers.h 先例）
//
// 官方魂印（DB effect_icon id=1806 / effect_id=2197，pet_id=4600）：
//   「战斗开始时，斯布林蒂会使己方所有其他精灵进行未来链接，未来链接初始为3级，
//    未来链接同时也视为未来奇点、未来指令参与层数、道数的计算（不占用原有上限与
//    触发原有效果）|斯布林蒂存活于出战背包时，己方处于未来链接的精灵计算受到的
//    攻击伤害时不计算克制倍数，并乘以己方处于未来链接的精灵中受克制最低者的
//    被克制倍数，触发后对应此被克制倍数的精灵减少1级未来链接|…」
//
// 为什么单独一个头：链接层数是**魂印侧**建立的每精灵状态，后续萨芙凯特（2198
// 未来指令）与逐界苍星（2041 未来奇点）的"链接视为奇点/指令参与层数、道数计算"
// 互认要跨魂印读它——把存储键与读写口放头文件，将来三族 include 同一份真相。
// 层数存 **pet.soulmark_storage**（下场保留；死亡/被击败清账走魂印侧 watcher）。
//
// ⚠️ 与克制倍数管线的接口只有 `ws.restraint_view[攻击方]`（battleWorkspace.h）：
//   链接替换 = 直写该视图为"池内最低被克制倍数"（不读原倍数 → 天然拦住任何
//   "克制倍数至少为N"的锁，如光之惩戒·英卡洛斯 1164——见 sibulindi_4600.cpp 头注）。
// ═══════════════════════════════════════════════════════════════════════════

#include <any>
#include <map>

#include <fsm/battleContext.h>

// 每个被链接精灵的层数槽（宿主 soulmark_storage；斯布林蒂本人不链接、无此键）。
constexpr int kFutureLinkLevelKey = 990219701;
// 初始等级（官方"未来链接初始为3级"，上限同为3——3级族效果按 ≥3 判）。
constexpr int kFutureLinkMaxLevel = 3;

// 取某只精灵当前的未来链接等级（无键 / 参数越界 = 0 级 = 未处于未来链接）。
inline int future_link_level(const BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return 0;
    }
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(kFutureLinkLevelKey);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 直写层数（调用方负责清账语义：死亡清 0、触发 -1；不在此钳上下限）。
inline void future_link_set_level(BattleContext* ctx, int side, int slot, int level) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ctx->seerRobot[side].elfPets[slot].soulmark_storage[kFutureLinkLevelKey] = level;
}

#endif  // FUTURE_LINK_H
