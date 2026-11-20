// K271 低体下回合先制+首战免死族 —— effect 347（1 宏 / 1 行，双子句）
//（神·哈迪斯 2713 args[2,0] kind[9]；号段 4270-4279，worktree ../agents/k271，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 347，2026-10-21）：
//   「①战斗阶段结束时，若自身体力低于对手体力，则下回合所有技能先制+2；
//     ②出战时，若首回合受到致命伤害则保留1点体力」
//   → db[0]=2 = 子句①先制量（"先制+2"字面自证）；db[1]=0 废位不消费（K1 纪律）；
//   子句②残留点数无参，按 tips 定值 1。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b347\b 无硬编码；②effect_info 347
//    无模板行；③soul_lib 全 worktree/registry 无 347 注册——真缺口。
// 实现：
//   ① BATTLE_ROUND_END 桶（=官方"战斗阶段结束时"，1011/K178 同款定例）：onstage 且
//   自身 hp>0 且 hp<对手 hp → 账本 plugin_storage[kGate+owner]=roundCount；次回合
//   BATTLE_FIRST_MOVE_RIGHT 桶：账本==roundCount-1 → ws.preemptive_level[owner]+=
//   db[0]（"下回合"一次性：648 账本形换锚——pl 的 memset 在 MOVE_RIGHT 时点，账本
//   只燃一回合，再评估再燃，SOP §二.2）。
//   ② OPERATION_ENTER_EXIT_STAGE（early，每次登场刷新）：记 plugin_storage
//   [kEntry+owner]=roundCount（K218 登场账同款）+ EVENT_HP_EXHAUSTED watcher
//   （own，TEAM，旗标防双挂）：roundCount==登场回合 → heal_amount(owner,1)。
//   信号模型天然只应伤害通道致死（秒杀/自爆自有信号面，帝皇之御口径）；无 boss
//   注记 → 不设 boss 门；免死逐次应答、时限=登场首回合（非次数）。
// 注册：SOUL_PROGRAM(347) 三节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4270_lowhp_preempt_entry_save.cpp（4270-4279）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 347;
constexpr int kGateKeyBase = 990771000;    // +owner：子句①账本（值=燃点 roundCount）
constexpr int kEntryKeyBase = 990772000;   // +owner：子句②登场回合
constexpr int kWiredKeyBase = 990773000;   // +owner：watcher 已挂旗标

// ① ROUND_END（战斗阶段结束）：低体门评估 → 账本记当轮。
EffectResult k271_347_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int self_hp = ctx->getPet(owner).hp;
    const int foe_hp = ctx->getPet(1 - owner).hp;
    if (self_hp > 0 && self_hp < foe_hp) {
        ctx->plugin_storage[kGateKeyBase + owner] = ctx->roundCount;
    }
    return EffectResult::kOk;
}

// ① FMR：账本是上一轮的 → 当回合先制 +db[0]（memset 后写）。
EffectResult k271_347_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto gate = ctx->plugin_storage.find(kGateKeyBase + owner);
    if (gate == ctx->plugin_storage.end() ||
        any_int_or(gate->second) != ctx->roundCount - 1) {
        return EffectResult::kOk;   // 无账本/非"下回合"（隔轮残留不燃）
    }
    int amount = 2;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            amount = db[0];
        }
    }
    ctx->ws.preemptive_level[owner] += amount;
    return EffectResult::kOk;
}

// ② watcher：登场首回合致死 → 拉起 1 点（信号模型，帝皇之御口径）。
void k271_on_hp_exhausted(BattleContext* c, int owner, const BattleEvent& ev) {
    if (!c || !g_core || !g_core->heal_amount) {
        return;
    }
    if (ev.target != owner) {
        return;
    }
    const auto entry = c->plugin_storage.find(kEntryKeyBase + owner);
    if (entry == c->plugin_storage.end() ||
        any_int_or(entry->second) != c->roundCount) {
        return;   // 非登场首回合
    }
    if (!onstage_has_soul_mark(c, owner, kMarkId)) {
        return;
    }
    (void)g_core->heal_amount(c, owner, 1);
}

// arm：登场账刷新 + 免死 watcher 接线（early 双触发同值幂等）。
EffectResult k271_347_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kEntryKeyBase + owner] = ctx->roundCount;
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_HP_EXHAUSTED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            k271_on_hp_exhausted(c, owner, ev);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（347 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k271_347_enter,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k271_347_round_end),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k271_347_move_right)});
