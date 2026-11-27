// K305 能力下降门必暴族 —— effect 290（1 宏 / 1 行）
//（黄金圣龙王 3248 args[3,100]；号段 4610-4619，worktree ../agents/k305，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 290，2026-10-21）：
//   「对手处于能力下降状态时自身攻击必定致命一击」
//   → db[1]=100 = 必暴概率%（"必定"=100 字面自证，roll 申报制）；db[0]=3 语义不明
//   不消费（板行已注明记档）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b290\b 无魂印硬编码；②effect_info
//    290 无模板行；③soul_lib 全 worktree/registry 无 290 注册——真缺口。
// 实现（板行处方：ws.must_crit 必暴通道（chaos_mozun 1263/681 同款））：
//   BATTLE_ROUND_START 桶：对手任一能力槽 <0（"处于能力下降状态"，6 槽全扫）→
//   roll_percent(db[1]) → ws.must_crit[owner] = true（回合内整回合有效）。⚠️ 时点
//   勘误（T4611 实测）：板行处方写的 SKILL_EFFECT 时点太晚——暴击判定在伤害管线
//   内、先于 SKILL_EFFECT 桶消费 must_crit（encarlos ②"命中前桶读"口径），改挂
//   ROUND_START（266 hit_rate_mod 每回合重写同型；门在回合开始评估记档）。
// 注册：SOUL_PROGRAM(290) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4610_debuff_mustcrit.cpp（号段 4610-4619）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 290;

// SKILL_EFFECT：对手下降态门 → roll → 必暴。
EffectResult k305_290_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    bool debuffed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && !debuffed; ++i) {
        debuffed = ctx->ability_levels[opp][i] < 0;
    }
    if (!debuffed) {
        return EffectResult::kOk;   // 门闭
    }
    int pct = 100;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 1 && db[1] > 0) {
            pct = db[1];
        }
    }
    if (!roll_soul_percent(*ctx, kMarkId, 0, owner, owner, pct)) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（290 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k305_290_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
