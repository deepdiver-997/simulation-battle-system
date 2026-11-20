// K315 受击双防+回合末回血族 —— effect 672（1 宏 / 1 行）
//（利刃之主鲁斯王 2947 args[1,1,1,3,1,5]；号段 4710-4719，worktree ../agents/k315，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 672，2026-10-21）：
//   「每次受到攻击则自身双防+1,且战斗阶段结束时恢复自身1/5HP(BOSS有效)」
//   → db[5]=5 = 回血分母（"1/5"字面自证）；双防+1 为 tips 定值（首部 1 位废位不消费，
//   K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b672\b 无魂印硬编码；②effect_info
//    672 技能空间模板不同文；③soul_lib 全 worktree/registry 无 672 注册——真缺口。
// 实现（板行处方：K301 1052 直系变体——去受击旗，回血无条件）：
//   ① EVENT_TAKE_DAMAGE watcher（own、只认被对手打）→ stat_change(防2/特防3, +1)
//   （AFTER_ACTION 时点记档同 K301）。
//   ② BATTLE_ROUND_END 桶：无条件 heal_amount(max/db[5])（恢复原语，封回血钳 max）。
//   BOSS 有效 → 无门。
// 注册：SOUL_PROGRAM(672) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4710_hitdef_heal_fifth.cpp（号段 4710-4719）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 672;
constexpr int kWiredKeyBase = 990815000;   // +owner：watcher 已挂旗标

// ROUND_END：无条件回血 max/db[5]。
EffectResult k315_672_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    int denom = 5;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 5 && db[5] > 0) {
            denom = db[5];
        }
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp > 0) {
        (void)g_core->heal_amount(ctx, owner, max_hp / denom);
    }
    return EffectResult::kOk;
}

// arm：受击 watcher（双防+1）。
EffectResult k315_672_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_change) {
                return;
            }
            if (ev.target != owner || ev.actor == owner) {
                return;   // 只认"被对手打"
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            (void)g_core->stat_change(c, owner, /*stat=*/2, /*boost=*/1);
            (void)g_core->stat_change(c, owner, /*stat=*/3, /*boost=*/1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（672 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k315_672_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k315_672_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
