#ifndef HAMO_WING_RUNTIME_H
#define HAMO_WING_RUNTIME_H

// 王之哈莫 3022（魂印 effect_icon 715）+ 重生之翼 2987（魂印 effect_icon 866）
// 运行时 —— 魂印侧（soul_lib/hamo_wing_family.cpp）与技能侧（moves_lib/wing_skills.cpp）
// 共享的**int 发布槽**键约定 + 读写助手。
//
// ⚠️ 全部状态用 **int 键 → int 值**（std::any 装 fundamental type）：魂印在 soul_plugin、
//    加层技能效果在 moves_plugin，两个 dylib + 场景测试三方都要读写同一份状态——
//    自定义结构体跨 RTLD_LOCAL dylib 的 any_cast typeinfo 不统一（4677 星盘踩过的坑），
//    fundamental type 全局唯一，安全。别把状态改成 struct（要加字段就加新键）。
//
// 存放位置 = **各 pet 自己的 soulmark_storage**（下场保留——锚点窗口/击杀挂起/固伤记录
// 都要跨切换存活）。镜像对战两边各有一只同名精灵时天然按 pet 隔离。
#include <any>
#include <map>

#include <fsm/battleContext.h>   // wing_adjust_layers 授/撤票用（inline，插件可调）

// ── 重生之翼（866）的 pet 级键 ───────────────────────────────────
// 键段 = 8660000 + N（一只重生一份；"神耀能量"是这只宠自己的资源）。
constexpr int kWingLayersKey = 8660001;          // 神耀能量层数（0..6，钳顶 6）
constexpr int kWingKillPendingKey = 8660002;     // 6 层击杀 bundle 挂起（0/1）
constexpr int kWingKillActiveRoundKey = 8660003; // 挂起 bundle 已吃到先制+3 的回合（-1 = 未激活）
constexpr int kWingActedOnceKey = 8660004;       // ≥3 层回血前提："自身出手起"（0/1）
constexpr int kWingPinkDebtKey = 8660005;        // ≥4 层：已记录待反弹的固伤累计（下场保留）
constexpr int kWingKillRoundKey = 8660006;       // 最近一次"击败对手"的回合（-1 = 无；865 技能读）
constexpr int kWingEnterRoundKey = 8660007;      // 最近一次登场的回合（-1 = 无；bundle 激活回避当回合）
constexpr int kWingReviveHpBaseKey = 8660010;    // 8660010+slot = 该槽游戏开始时体力（复活口径 idx=224）
constexpr int kWingPipelineValidKey = 8660030;   // 减伤管线条目防重复注册（存上次注册的 pipeline_valid_id）
constexpr int kWingReviveRecordedKey = 8660900;  // 游戏开始体力快照已做（0/1）
constexpr int kWingWatchersKey = 8660901;        // watcher 只挂一次（0/1）

// ≥2 闭环免控/免弱票的固定句柄（grant source_id 全局唯一；层越线时授/撤）
constexpr int kWingHandleAnomaly = 8660091;
constexpr int kWingHandleStatDrop = 8660092;

// ── 王之哈莫（715）的 pet 级键 ───────────────────────────────────
// 键段 = 7150000 + N。
constexpr int kHamoJudgeRoundKey = 7150001;        // 最近一次回合开始判定的回合（-1 = 未判定）
constexpr int kHamoHighKey = 7150002;              // 本轮判定：体力高于对手（0/1）
constexpr int kHamoLowKey = 7150003;               // 本轮判定：体力低于对手（0/1）
constexpr int kHamoLowConfirmedRoundKey = 7150004; // 低体回血已确认的回合（命中前锚点写；28 时点消费）
constexpr int kHamoPipelineValidKey = 7150005;     // 减伤管线条目防重复注册
constexpr int kHamoWatchersKey = 7150900;          // watcher 只挂一次（0/1）

// 免控窗口票固定句柄（窗口 = 本轮命中前 → 下轮命中前，每轮锚点先撤后授）
constexpr int kHamoHandleWindow = 7150091;

// ── int 发布槽读写助手（缺 key 视为 0 / def）─────────────────────
inline int hamo_wing_get(const std::map<int, std::any>& storage, int key, int def = 0) {
    const auto it = storage.find(key);
    if (it == storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
inline void hamo_wing_set(std::map<int, std::any>& storage, int key, int value) {
    storage[key] = value;
}

inline int wing_layers(const std::map<int, std::any>& storage) {
    return hamo_wing_get(storage, kWingLayersKey);
}

// 神耀能量层变动的**唯一入口**（soul 侧清零/技能侧加耗层都必须走它）：
// 钳到 [0,6]，并在 ≥2 线上授/撤"闭环免控免弱"票——语料"免控免弱是开启后**闭环
// 免控**，直到层数低于2为止"。票 = 全时点 coverage + soul(魂免) + Ancient 层
// （两只都在《浅谈主动毒与魂印免控判定》老魂免补丁名单）+ TEAM（**下场保留**：
// 击杀 bundle 的"一小段免控"靠它跨切换存活；perform_switch 只清 ON_STAGE）。
inline void wing_adjust_layers(BattleContext* ctx, int owner, int slot, int delta) {
    if (!ctx || owner < 0 || owner > 1 || slot < 0 || slot > 5 || delta == 0) {
        return;
    }
    std::map<int, std::any>& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
    const int old_layers = wing_layers(storage);
    int new_layers = old_layers + delta;
    if (new_layers < 0) {
        new_layers = 0;
    }
    if (new_layers > 6) {
        new_layers = 6;
    }
    if (new_layers == old_layers) {
        return;
    }
    storage[kWingLayersKey] = new_layers;
    if (old_layers < 2 && new_layers >= 2) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/0, kWingHandleAnomaly,
                            /*soul=*/true, EffectScope::TEAM, /*counts=*/0, /*effect_id=*/866,
                            ImmunityTier::Ancient);
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                            0, 0, kWingHandleStatDrop, /*soul=*/true, EffectScope::TEAM,
                            0, 866, ImmunityTier::Ancient);
    } else if (old_layers >= 2 && new_layers < 2) {
        ctx->revoke_immunity(owner, kWingHandleAnomaly);
        ctx->revoke_immunity(owner, kWingHandleStatDrop);
    }
}

// 消耗全部层数（862"消耗自身所有神耀能量"；击杀 bundle 清零同款）→ 返回消耗前的层数。
inline int wing_consume_all_layers(BattleContext* ctx, int owner, int slot) {
    const int old_layers = wing_layers(ctx->seerRobot[owner].elfPets[slot].soulmark_storage);
    wing_adjust_layers(ctx, owner, slot, -old_layers);
    return old_layers;
}

#endif  // HAMO_WING_RUNTIME_H
