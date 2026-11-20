// K206 异常负槽回血族 —— effect 295（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 295，2026-10-07）：
//   295 龙胆天神 2576 args[]（零参）：「若自己处于异常或者能力下降状态，恢复150点HP」
//
// ⚠️ 实现形态（T4 类型化行参）：ROUND_START 节点（K163 261 "每回合恢复"同槽位形态，
//    STAGE 作用域）——onstage 宿主且 [has_any_active_abnormal_status(owner) 或
//    ability_levels 任一槽 <0] → heal_amount(150)。回合首即回（无掷点、零参）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 295 魂印行为硬编码 ②effect_info
//    无 295 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k_open_window_immune 261（ROUND_START 回血）、K165（负槽门循环）、K176（异常门）。
// 场景回归：test/scenario/scenario_3455_cond_heal.cpp（号段 3455-3464）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 295;

bool has_any_drop(const BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            return true;
        }
    }
    return false;
}

EffectResult k206_295_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(owner) && !has_any_drop(ctx, owner)) {
        return EffectResult::kOk;   // 门：无异常且无负槽 → 不回
    }
    (void)g_core->heal_amount(ctx, owner, 150);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（295 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k206_295_round_start)});
