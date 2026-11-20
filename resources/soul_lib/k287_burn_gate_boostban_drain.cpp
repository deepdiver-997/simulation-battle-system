// K287 烧伤门禁提升+周期吸取族 —— effect 657（1 宏 / 1 行）
//（妮可萝 2932 args[2,4]；号段 4430-4439，worktree ../agents/k287，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 657，2026-10-21）：
//   「若对手处于烧伤状态,则无法进行能力提升并且每回合{吸取}对手1/4最大{体力}」
//   → db[0]=2 = 门异常码（烧伤 Burn=2 字面自证）；db[1]=4 = 吸取分母（"1/4"字面
//   自证）——双参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b657\b 无魂印硬编码；②effect_info
//    657 无模板行；③soul_lib 全 worktree/registry 无 657 注册——真缺口。
// 实现（板行处方：K255 320 异常门 + STAT_BOOST 免疫 + drain_hp 周期吸取）：
//   BATTLE_ROUND_START 桶（"每回合"）：对手处于烧伤（门码 db[0]）→
//   ① grant_immunity(对手, STAT_BOOST, coverage_all, duration=1)——"无法进行能力
//     提升"（chaos_sorensen ①-2 同款票；IMMUNE 天然不可被断回合、随上下场清；每
//     ROUND_START 重授，门闭后残留 ≤1 回合记档）；
//   ② drain_hp(owner, 对手, db[1])——"每回合吸取对手 1/4 最大体力"（目标掉
//     max/denom、自己恢复等量，原语内 clamp）。
//   门闭（对手无烧伤）→ 两 rider 均不授。对手已倒不磨血（口径选择记档）。
// 注册：SOUL_PROGRAM(657) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4430_burn_gate_boostban_drain.cpp（4430-4439）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 657;

// 行参：db[0]=门异常码（兜底 Burn）、db[1]=吸取分母（兜底 4）。
void k287_read_row(BattleContext* ctx, int owner, int& gate_code, int& denom) {
    gate_code = static_cast<int>(AbnormalStatusId::Burn);
    denom = 4;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        gate_code = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        denom = db[1];
    }
}

// ROUND_START：对手烧伤门 → 禁提升票 + 周期吸取。
EffectResult k287_657_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：不磨血（记档）
    }
    int gate_code = static_cast<int>(AbnormalStatusId::Burn);
    int denom = 4;
    k287_read_row(ctx, owner, gate_code, denom);
    if (!g_core->has_active_anomaly(ctx, opp, gate_code)) {
        return EffectResult::kOk;   // 门闭：不授不禁
    }
    // ① 禁提升票（chaos_sorensen ①-2 同款；duration=1 每回合重授，随上下场清）。
    ctx->grant_immunity(opp, ImmunityType::STAT_BOOST, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/kMarkId);
    // ② 周期吸取 1/db[1] 最大体力（目标掉血、自己等量恢复）。
    (void)g_core->drain_hp(ctx, owner, opp, denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（657 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k287_657_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
