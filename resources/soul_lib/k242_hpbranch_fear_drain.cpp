// K242 双分支概率害怕/吸取族 —— effect 1414（1 宏 / 1 行）
//（幽冥魅影 3828；author k-prod-6 2026-10-08 立单，号段 3860-3869）
//
// 行清单（tools/soul_effects_dump.py 1414，2026-10-13）：
//   row1103 args[75,1,3,1]（kind[5,10,12]）
//   「战斗阶段结束时若自身体力高于对手则有75%概率使对手害怕，未触发则使对手全属性-1；
//    战斗阶段结束时若自身体力低于对手则75%概率{吸取}对手最大{体力}的1/3，
//    未触发则使对手全属性-1（boss无效）」
//   → db[0]=两分支共用概率 75；db[1]/db[3] 恒 1 无对应子句——不消费，记档；
//     db[2]=吸取分母（1/3）。
//
// 实现（BATTLE_ROUND_END 桶单节点，"战斗阶段结束时"=chaos_sorensen ④ 同锚）：
//   · 高体（自身在场宠 hp 严格 > 对手）：apply_anomaly_checked(害怕6,
//     random_anomaly_duration, own, db[0], SoulMark)（申报制）→ ROLL_FAILED
//     （仅此档，K172 口径）→ 全属性-1；
//   · 低体（严格 <）：roll_percent(db[0])（K399 手掷形）→ 成功：吸取双段
//     deal_pink_damage(opp, max_hp(opp)/db[2], PERCENT_VALUE, own) +
//     heal_amount(own, 同值)（k225 341 形；回复按封顶、粉伤全额）；
//     未掷中 → 全属性-1；
//   · 相等：两分支都不触发（严格对照）；全属性 = 六槽含命中（chaos_sorensen
//     口径），stat_drop 逐槽（k893 形，免弱原语自查）。
// boss 无效标注：tips 尾注 → 节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-13）：①内核无 1414 魂印硬编码；②effect_info 1414
//    同号模板不同文（错位行，按行 tips 落地）；③soul_lib/registry 无 1414 注册——真缺口。
// 先例：chaos_sorensen ④（ROUND_END 锚）、K229（HP 对照门）、K172（申报制+ROLL_FAILED
//   补偿）、k893（全属性 stat_drop 循环）、k225（吸取双段值形）。
// 场景回归：test/scenario/scenario_3860_hpbranch_fear_drain.cpp（号段 3860-3869）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1414;
constexpr int kCombatStatCount = 6;

struct K242Row {
    int chance = 75;
    int drain_denom = 3;
};

K242Row k242_read_row(BattleContext* ctx, int owner) {
    K242Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.chance = db[0];
    }
    if (db.size() > 2 && db[2] > 0) {
        row.drain_denom = db[2];
    }
    return row;
}

// 全属性-1（六槽含命中；stat_drop 自查免弱，k893 形）。
void drop_all_stats(BattleContext* ctx, int owner, int target) {
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < kCombatStatCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, 1, owner);
    }
}

// ── 战斗阶段结束：HP 对照双分支 ──────────────────────────────────────────────

EffectResult k242_1414_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->apply_anomaly_checked
        || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(ctx->opponent(owner));
    if (self.hp <= 0 || foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const K242Row row = k242_read_row(ctx, owner);
    const int opp = ctx->opponent(owner);

    if (self.hp > foe.hp) {
        // 高体：75% 害怕；未触发 → 全属性-1
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), owner, row.chance, ChanceSource::SoulMark);
        if (r == ApplyAnomalyResult::ROLL_FAILED) {
            drop_all_stats(ctx, owner, opp);
        }
        return EffectResult::kOk;
    }
    if (self.hp < foe.hp) {
        // 低体：75% 吸取对手 1/db[2] maxHP；未掷中 → 全属性-1
        if (roll_soul_percent(*ctx, kMarkId, 0, owner, opp, row.chance)) {
            const int drain = resolve_pet_max_hp(foe) / row.drain_denom;
            if (drain > 0) {
                (void)g_core->deal_pink_damage(ctx, opp, drain,
                                               DamageKind::PERCENT_VALUE, owner);
                (void)g_core->heal_amount(ctx, owner, drain);
            }
        } else {
            drop_all_stats(ctx, owner, opp);
        }
    }
    return EffectResult::kOk;   // 相等：两分支都不触发（严格对照）
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1414 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k242_1414_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
