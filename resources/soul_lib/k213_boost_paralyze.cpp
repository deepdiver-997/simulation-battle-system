// K213 强化攻击麻痹族 —— effect 316（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 316，2026-10-07）：
//   316 光辉·米瑞斯 2639 args[2, 15]：「自身处于强化状态时，每次攻击都有15%的概率
//     使对手麻痹」→ args 语义存疑不消费（记档）；15=概率由 tips 承载。
//
// ⚠️ 实现形态（T4 类型化行参）：EVENT_SKILL_USED watcher（K176 同窗，arm-once）：
//    own + 攻击技（!used_attribute）+ 自身任一正槽（强化门，K165 门取反）→
//    apply_anomaly_checked(麻痹0, 15%, SoulMark)——申报通道不预掷。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 316 魂印行为硬编码 ②effect_info
//    无 316 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k176_branch_anomaly（SKILL_USED 概率申报窗全形态）、k165（正/负槽门循环）。
// 场景回归：test/scenario/scenario_3560_boost_paralyze.cpp（号段 3560-3569）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>

namespace {

constexpr int kMarkId = 316;
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);   // 0

bool self_has_boost(const BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

EffectResult k213_316_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const auto key = static_cast<unsigned long long>(990213000 + owner);
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    const int chance = 15;   // tips 字面（args 存疑不消费）
    const int opp = ctx->opponent(owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, chance](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)
                || !self_has_boost(wctx, owner)) {
                return;   // 强化门（受击时点现行值）
            }
            (void)g_core->apply_anomaly_checked(wctx, opp, kParalysis,
                                                random_anomaly_duration(), owner,
                                                chance, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（316 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k213_316_arm,
                        /*once=*/false, /*early=*/true)});
