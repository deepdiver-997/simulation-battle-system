// K208 暴击回满族 —— effect 322（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 322，2026-10-07）：
//   322 剑皇 2651 args[100, 100]：「造成致命一击时恢复自身全部体力！」
//     → args 恒满档 datum（回满语义由 tips 字面承载，arg 不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：EVENT_TAKE_DAMAGE watcher（K200 直系，arm-once）：
//    ev.target==opp 且 ev.actor==owner 且 ev.amount>0 且 crit_happened[owner]
//    → heal_amount(resolve_pet_max_hp - hp)（回满；封顶由原语兜底）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 322 魂印行为硬编码 ②effect_info
//    无 322 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k200_crit_drain（watcher + crit 门全形态）。
// 场景回归：test/scenario/scenario_3490_crit_fullheal.cpp（号段 3490-3499）。

#include "common.h"

namespace {

constexpr int kMarkId = 322;

struct K208Runtime {
    bool watcher_armed = false;
};

K208Runtime& k208_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K208Runtime{};
    }
    return std::any_cast<K208Runtime&>(storage[kMarkId]);
}

EffectResult k208_322_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K208Runtime& rt = k208_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->heal_amount) {
                return;
            }
            const int opp = wctx->opponent(owner);
            if (ev.target != opp || ev.actor != owner || ev.amount <= 0) {
                return;
            }
            if (!wctx->crit_happened[owner]) {
                return;   // 暴击门
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            const int deficit = resolve_pet_max_hp(self) - self.hp;
            if (deficit > 0) {
                (void)g_core->heal_amount(wctx, owner, deficit);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（322 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k208_322_arm,
                        /*once=*/false, /*early=*/true)});
