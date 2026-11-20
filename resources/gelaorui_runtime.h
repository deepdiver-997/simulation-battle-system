#ifndef GELAORUI_RUNTIME_H
#define GELAORUI_RUNTIME_H

// 圣光格劳瑞（神谕 4004）运行时 —— 魂印侧（soul_lib/shengguang_gelaorui_2532.cpp）
// 与技能侧（moves_lib/shengguang_gelaorui_skills.cpp）共享的霆/裁计数。
// 跨 dylib int 发布槽纪律（oracle_runtime.h 同款）。
//
// 口径（effect_icon 2121 / effect 2532 + 语料 idx=19）：
//   神圣之霆 = 自身 0..6 层；登场 +1、第五击杀 +1；满 6 下场保留、未满清（本轮先按
//              保留处理，待实测）；每次麻痹触发成功本场获得数 +1（简化为触发即 +1 层预算）。
//   荣光之裁 = 绑对手（对方 side 计数），持有者：有免疫攻击效果则耗 1 层令其失效、
//              视为不处于异常（K6 mask）。下场保留、官方未写上限（先按 6 封顶）。

#include <any>
#include <fsm/battleContext.h>
#include <plugin/core_api.h>

constexpr int kGlaPubTing = 4692000;   // + side：神圣之霆层数（0..6）
// 荣光之裁层数（挂在 side 身上，0..6）——**K6 虚拟异常视图**（has_active_anomaly
// 查询时拉取，>0 → 该方视为不处于异常，被消耗归零自然解除）。
// 2026-10-24 转正：状态一等化到 BattleContext::anomaly_view_layers，经 CoreApi
// set/get_anomaly_view_layers 槽读写（原 plugin_storage[4699000+2532*8+side] 魔数段
// 退役，键位撞号审计从此覆盖 effect_id）。helper 显式收 core 指针：插件 TU 传
// g_core（common.h），场景 harness TU 传 core_api() 单例地址。
constexpr int kGlaMarkId = 2532;

inline int gla_cai_get(const CoreApi* core, BattleContext* ctx, int side) {
    if (ctx && core && core->get_anomaly_view_layers) {
        return core->get_anomaly_view_layers(ctx, kGlaMarkId, side);
    }
    return 0;
}

inline void gla_cai_set(const CoreApi* core, BattleContext* ctx, int side, int layers) {
    if (ctx && core && core->set_anomaly_view_layers) {
        core->set_anomaly_view_layers(ctx, kGlaMarkId, side, layers);
    }
}

// ── 2026-09-25 扩展（交接文档四'遗留批）──
// 魂印"对手下1次…"弹控三条 + 2551 后半句 + 王·万雷禁法"发首个标记"（键=pet_id）。
constexpr int kGlaPubTrapAttack = 4692100;  // + side：对手下1次**攻击技**→异常转麻痹（armed=1）
constexpr int kGlaPubTrapAttr = 4692200;    // + side：对手下1次**属性技**→麻痹1回合（armed=1）
constexpr int kGlaPubTrapIdle = 4692300;    // + side：对手下1次未用技→下回合攻先≤0（armed=1）
constexpr int kGlaPubIdleUntil = 4692400;   // + side：攻先≤0 生效至该回合（ROUND_START 写 ws）
constexpr int kGlaPubBlind = 4692500;       // + side：2551 击败武装（对手属性技命中失效）
constexpr int kGlaPubBlindTing = 4692600;   // + side：武装时的霆层数快照（霆减少 → 解除）
constexpr int kGlaPubMarkBase = 4692700;    // + pet_id：精灵王"首个标记"（王·万雷禁法 =5 只发放）

inline int gla_pub_get(const std::map<int, std::any>& store, int key) {
    auto it = store.find(key);
    if (it == store.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

inline void gla_pub_set(std::map<int, std::any>& store, int key, int value) {
    store[key] = value;
}

#endif // GELAORUI_RUNTIME_H
