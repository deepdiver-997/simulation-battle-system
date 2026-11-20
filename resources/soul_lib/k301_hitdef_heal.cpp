// K301 挨打双防+挨打回血族 —— effect 1052（1 宏 / 1 行）
//（布姆 3453 args[1,4]；号段 4570-4579，worktree ../agents/k301，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 1052，2026-10-21）：
//   「受到攻击时自身双防+1；当回合未受到攻击则恢复自身最大体力的1/4」
//   → db[0]=1 = 双防提升量（"+1"字面互证）；db[1]=4 = 回血分母（"1/4"字面自证）——
//   双参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b1052\b 无魂印硬编码；②effect_info
//    1052 无模板行；③soul_lib 全 worktree/registry 无 1052 注册——真缺口。
// 实现（板行处方：1604 同型布局——受击事件面 + 回合末账本）：
//   ① EVENT_TAKE_DAMAGE watcher（own、只认被对手打 ev.target==owner && ev.actor!=owner，
//   K2 262 同门）→ stat_change(防槽2,+db[0]) + stat_change(特防槽3,+db[0])（自增益
//   通道不查免弱）。⚠️ 时点记档：TAKE_DAMAGE watcher 实际在 AFTER_ACTION 跑（SOP
//   §三.1）——当回合后续伤害不享新防，官方即时性可能有偏差，读快照族同口径接受。
//   ② "当回合未受到攻击则恢复"：watcher 同时置受击旗（plugin_storage）；ROUND_END
//   桶查旗——无旗 → heal_amount(max/db[1])；无论有无旗，回合末清旗（下回合重计）。
// 注册：SOUL_PROGRAM(1052) 双节点 + watcher，总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4570_hitdef_heal.cpp（号段 4570-4579）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1052;
constexpr int kHitFlagKeyBase = 990801000;   // +owner：当回合受击旗
constexpr int kWiredKeyBase = 990802000;     // +owner：watcher 已挂旗标

// 行参：db[0]=双防提升量（兜底 1）、db[1]=回血分母（兜底 4）。
void k301_read_row(BattleContext* ctx, int owner, int& boost, int& denom) {
    boost = 1;
    denom = 4;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        boost = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        denom = db[1];
    }
}

// arm：接受击 watcher（双防+1 + 受击旗）。
EffectResult k301_1052_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
                return;   // 只认"被对手打"（K2 262 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int boost = 1;
            int denom = 4;
            k301_read_row(c, owner, boost, denom);
            c->plugin_storage[kHitFlagKeyBase + owner] = 1;   // 受击旗（②句用）
            (void)g_core->stat_change(c, owner, /*stat=*/2, boost);   // 防御
            (void)g_core->stat_change(c, owner, /*stat=*/3, boost);   // 特防
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② ROUND_END：当回合未受击 → 回血 max/db[1]；清旗。
EffectResult k301_1052_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int key = kHitFlagKeyBase + owner;
    const bool hit = ctx->plugin_storage.count(key) > 0;
    ctx->plugin_storage.erase(key);   // 无论分支，回合末清旗（下回合重计）
    if (hit) {
        return EffectResult::kOk;   // 当回合受过攻击 → 不回血
    }
    int boost = 1;
    int denom = 4;
    k301_read_row(ctx, owner, boost, denom);
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (self.hp > 0 && max_hp > 0) {
        (void)g_core->heal_amount(ctx, owner, max_hp / denom);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1052 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k301_1052_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k301_1052_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
