// K199 受击消强族 —— effect 180（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 180，2026-10-07）：
//   180 天河神将 2735 args[0, 0]：「受到攻击将消除对手能力提升状态」
//     → db[0]/db[1] 恒 0（官方 tips 无对应子句，不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：TAKE_DAMAGE watcher（1067 同门 arm-once，TEAM 永久）
//    ——ev.target==owner 且 ev.actor!=owner 且 ev.amount>0（"受到攻击"取受击且确有
//    伤害落地的现值口径，与本夜 K190/K179 家族一致——零伤/被挡不触发，记档）
//    → clear_stat_boosts(ev.actor 侧)（消强原语，每次受击触发）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 180 魂印行为硬编码 ②effect_info
//    180 同号 args_num=0（技能空间同号，魂印以 effect_icon tips 为权威）③soul_lib
//    并集无同 id 注册——真缺口。
// 先例：undying_1067（TAKE_DAMAGE watcher arm-once 全形态）、yanshou（break/clear 原语）。
// 场景回归：test/scenario/scenario_3325_hit_clear_boost.cpp（号段 3325-3334）。

#include "common.h"

namespace {

constexpr int kMarkId = 180;

struct K199Runtime {
    bool watcher_armed = false;
};

K199Runtime& k199_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K199Runtime{};
    }
    return std::any_cast<K199Runtime&>(storage[kMarkId]);
}

EffectResult k199_180_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K199Runtime& rt = k199_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->clear_stat_boosts) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 仅对手出手且确有伤害（1067 同门 + 现值口径）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            (void)g_core->clear_stat_boosts(wctx, ev.actor);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（180 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k199_180_arm,
                        /*once=*/false, /*early=*/true)});
