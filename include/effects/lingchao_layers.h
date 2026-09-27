#ifndef LINGCHAO_LAYERS_H
#define LINGCHAO_LAYERS_H

// ═══════════════════════════════════════════════════════════════════════════
// 灵巢之主·索杰德尔 4599 —— **命起 / 魂落 双标记共享层**
//
// 为什么单独一个头（同 curse_stack.h 先例）：这两个标记是**魂印侧**（soul_lib）建立的
// 状态、却被**技能侧**（moves_lib）改写——命诞如虚/魂殒若梦各带"为自身增加1层命起/魂落"，
// 两库不链接彼此、也不能共享 .cpp 里的匿名 namespace 常量。把存储键与读写口放头文件，
// 两侧 include 同一份真相（插件只调 inline 入口，不链接 core 符号）。
//
// 语义（官方 4399 pet_id 4599 + 机制解析 idx=389）：
//   · 二者层数之和恒为 4（一项 +1 → 另一项 -1）；
//   · 战斗开始 2/2；阵亡即丢（idx=389 #1：被穿死后复活层数清零，0 威力技能可再拿）；
//   · 层数存 **pet.soulmark_storage**（下场保留）。
// ═══════════════════════════════════════════════════════════════════════════

#include <any>
#include <map>

#include <fsm/battleContext.h>

// 宿主 soulmark_storage 槽（与魂印 mark_id*100+n 系键错开；soul_lib 的
// lingchao_4599.cpp 用同一对键——改这里即两侧生效）。
constexpr int kLingchaoMingqiKey = 990459921;   // 命起层数
constexpr int kLingchaoHunluoKey = 990459922;   // 魂落层数
constexpr int kLingchaoLayerSum = 4;            // 两项之和恒为 4

// 取层数（不存在 = 0 层：阵亡清零后未重新获得，或该宠根本不是灵巢）。
inline int lingchao_layers(BattleContext* ctx, int side, int slot, int which_key) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return 0;
    }
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(which_key);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 写层数（直接落账，不加和约束——调用方负责镜像另一项）。
inline void lingchao_set_layers(BattleContext* ctx, int side, int slot, int mingqi, int hunluo) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    storage[kLingchaoMingqiKey] = mingqi;
    storage[kLingchaoHunluoKey] = hunluo;
}

// 定位本方灵巢槽（只剔消逝；阵亡照算——技能也可能在死后结算前读取）。
inline int lingchao_find_slot(const BattleContext* ctx, int side, int mark_id) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        if (!ctx->is_vanished(side, slot) && pet.soulMark.id == mark_id) {
            return slot;
        }
    }
    return -1;
}

#endif  // LINGCHAO_LAYERS_H
