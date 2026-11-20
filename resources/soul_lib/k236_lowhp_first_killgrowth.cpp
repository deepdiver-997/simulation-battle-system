// K236 低体先制+击杀成长族 —— effect 648（1 宏 / 1 行）
//（武极梵天 2911；author k-prod-6 2026-10-08 立单，号段 3800-3809）
//
// 行清单（tools/soul_effects_dump.py 648，2026-10-08）：
//   row317 args[1,2]（kind[9,11]）
//   「回合开始时体力低于对手，所有技能先制+1；每次击败对手自身攻击+2（boss无效）」
//   → db[0]=先制量（1）、db[1]=击败攻击成长（2）。
//
// 实现：
//   ① ROUND_START 桶：onstage 且自身在场宠 hp < 对手在场宠 hp → 门旗账本
//     （plugin_storage 记 roundCount）；
//     BATTLE_FIRST_MOVE_RIGHT 桶：门旗 roundCount 匹配当轮 →
//     ws.preemptive_level[owner] += db[0]（当回合所有技能先制+N）。
//     ⚠️ 时点口径（1576 实测记档/K6）：ws.preemptive_level 的 memset 在 MOVE_RIGHT
//     时点——ROUND_START 直写会被清，必须"ROUND_START 评估门 → 账本 → FMR 写"；
//     门在回合开始评估（ROUND_START 桶的其它回血效果会改 hp，FMR 处重评≠官方时点）。
//   ② BATTLE_AFTER_DEFEATED 桶（双侧、每杀必经）：find_defeated_foe_slot 归因门
//     （common.h：对手在场宠倒地 + 最后一击来自 owner 方）→
//     stat_change(owner, 攻槽0, +db[1])（"攻击"=槽 0，k211 315 先例）。
// boss 无效标注：tips 尾注 → 全节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-08）：①内核无 648 硬编码；②effect_info 648 同号模板
//    不同文（错位行，按行 tips 落地）；③soul_lib/registry 无 648 注册——真缺口。
// 先例：1576/1843A（ROUND_START 门+账本+FMR 写 pl、memset 记档）、K6 319（击杀归因
//   BATTLE_AFTER_DEFEATED + find_defeated_foe_slot）、k211（"攻击"槽 0）。
// 场景回归：test/scenario/scenario_3800_lowhp_first_killgrowth.cpp（号段 3800-3809）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 648;
constexpr int kGateBase = 990760400;   // +owner：低体先制门旗（值=通过的 roundCount）

struct K236Row {
    int first_amount = 1;
    int growth = 2;
};

K236Row k236_read_row(BattleContext* ctx, int owner) {
    K236Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.first_amount = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.growth = db[1];
    }
    return row;
}

// ── ① ROUND_START：低体门评估 → 账本 ────────────────────────────────────────

EffectResult k236_648_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int self_hp = ctx->getPet(owner).hp;
    const int foe_hp = ctx->getPet(ctx->opponent(owner)).hp;
    if (self_hp > 0 && self_hp < foe_hp) {
        ctx->plugin_storage[kGateBase + owner] = ctx->roundCount;
    }
    return EffectResult::kOk;
}

// ── ① FMR：门旗当轮有效 → 当回合先制 +N（memset 后写）────────────────────────

EffectResult k236_648_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto gate = ctx->plugin_storage.find(kGateBase + owner);
    if (gate == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    if (any_int_or(gate->second) != ctx->roundCount) {
        ctx->plugin_storage.erase(gate);   // 过期门旗（上轮残留）清除
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += k236_read_row(ctx, owner).first_amount;
    return EffectResult::kOk;
}

// ── ② 击败对手 → 自身攻击 +N（归因门）─────────────────────────────────────────

EffectResult k236_648_after_defeated(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (find_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 本时点无 owner 方的击败（K6/SOP §三.3 归因门）
    }
    (void)g_core->stat_change(ctx, owner, /*stat=*/0, k236_read_row(ctx, owner).growth);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（648 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k236_648_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k236_648_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_AFTER_DEFEATED, &k236_648_after_defeated,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
