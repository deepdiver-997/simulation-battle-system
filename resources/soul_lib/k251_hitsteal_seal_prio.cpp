// K251 受击吸取提升/级联封属先制族 —— effect 882（1 宏 / 1 行）
//（暗地魔影·格莱奥丹 3268；author k-prod-6 2026-10-13 立单，号段 4010-4019）
//
// 行清单（tools/soul_effects_dump.py 882，2026-10-13）：
//   row595 args[2,2,1,1]（kind[9,12,13]）
//   「受到攻击时吸取对手能力提升状态，若对手不处于能力提升状态则使对手下2回合属性
//    技能无效，吸取成功则自身下2回合先制+1（BOSS无效）」
//   → db[0]=封属回合（2）、db[1]=先制窗回合（2）、db[2]=先制量（+1）；
//     db[3] 恒 1 无对应子句——不消费，记档。
//
// 实现（K228/1456 已验证形态的直接复用——同作者家族子集：无石化掷点、无吸血）：
//   · EVENT_TAKE_DAMAGE watcher（K199 同门：ev.target==own && ev.actor!=own &&
//     ev.amount>0，own 存活守卫）：
//     - 对手有提升（六槽判据）→ transfer_stat_boosts 吸取；搬走>0 = 吸取成功 →
//       先制窗账本 until=round+db[1]（被免消除强化挡下=未成功，无补偿分支，记档）；
//     - 对手无提升 → 封属窗账本 until=round+db[0]。
//   · BATTLE_FIRST_MOVE_RIGHT 桶（onstage 守卫）：
//     - 先制：账本活 → ws.preemptive_level[own] += db[2]（memset 后写）；
//     - 封属：**单次授予** duration=db[0]（notify 只认 remaining_rounds>0、tick 每轮
//       自灭——每轮重授会越窗，K228 R4 实测记档）+ 已授旗防重复。
// boss 无效标注：tips 尾注 → 全节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-13）：①内核无 882 硬编码；②effect_info 882 同号模板
//    不同文（args_num=1 错位行，按行 tips 落地）；③soul_lib/registry 无 882 注册——真缺口。
// 先例：k228_hit_drain_petrify_seal（受击吸强/封属/先制窗全形态直系）、k199（watcher 形）。
// 场景回归：test/scenario/scenario_4010_hitsteal_seal_prio.cpp（号段 4010-4019）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 882;
constexpr int kLedgerPrioBase = 990760700;   // +owner：先制窗 until_round
constexpr int kLedgerSealBase = 990760800;   // +owner：封属窗 until_round
constexpr int kLedgerSealDoneBase = 990760900;  // +owner：封属已授旗
constexpr int kCombatStatCount = 6;

struct K251Runtime {
    bool watcher_armed = false;
};

K251Runtime& k251_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K251Runtime{};
    }
    return std::any_cast<K251Runtime&>(storage[kMarkId]);
}

struct K251Row {
    int seal_rounds = 2;
    int prio_rounds = 2;
    int prio_amount = 1;
};

K251Row k251_read_row(BattleContext* ctx, int owner) {
    K251Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.seal_rounds = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.prio_rounds = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        row.prio_amount = db[2];
    }
    return row;
}

bool side_has_any_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 受击：吸强 / 封属账本 ─────────────────────────────────────────────────────

EffectResult k251_882_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K251Runtime& rt = k251_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const K251Row row = k251_read_row(ctx, owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, row](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || ev.target != owner || ev.actor == owner
                || ev.amount <= 0 || ev.actor < 0 || ev.actor > 1) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            if (wctx->getPet(owner).hp <= 0) {
                return;
            }
            if (side_has_any_boost(wctx, ev.actor)) {
                if (g_core->transfer_stat_boosts(wctx, ev.actor, owner) > 0) {
                    // 吸取成功 → 下 db[1] 回合先制 +db[2]（本回合不算）
                    wctx->plugin_storage[kLedgerPrioBase + owner] =
                        wctx->roundCount + row.prio_rounds;
                }
            } else {
                // 对手无提升 → 下 db[0] 回合属性技无效
                wctx->plugin_storage[kLedgerSealBase + owner] =
                    wctx->roundCount + row.seal_rounds;
                wctx->plugin_storage.erase(kLedgerSealDoneBase + owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── FMR：先制重写 / 封属单授 ─────────────────────────────────────────────────

EffectResult k251_882_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K251Row row = k251_read_row(ctx, owner);
    const auto prio = ctx->plugin_storage.find(kLedgerPrioBase + owner);
    if (prio != ctx->plugin_storage.end()
        && any_int_or(prio->second) >= ctx->roundCount) {
        ctx->ws.preemptive_level[owner] += row.prio_amount;
    }
    const auto seal = ctx->plugin_storage.find(kLedgerSealBase + owner);
    const auto seal_done = ctx->plugin_storage.find(kLedgerSealDoneBase + owner);
    if (seal != ctx->plugin_storage.end()
        && seal_done == ctx->plugin_storage.end()
        && any_int_or(seal->second) >= ctx->roundCount) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/ctx->opponent(owner),
                                 /*effect_id=*/kMarkId, /*attribute=*/true,
                                 /*attack=*/false, /*count=*/0,
                                 /*duration_rounds=*/row.seal_rounds,
                                 /*penetrable=*/true, /*source_slot=*/-1,
                                 EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                 /*chance_pct=*/100, /*consumed_when_pierced=*/false);
        ctx->plugin_storage[kLedgerSealDoneBase + owner] = 1;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（882 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k251_882_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k251_882_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
