// K194 暴击+吸血族 —— effect 619（1 宏 / 3 行）
//                                                                          （2026-10-07，worktree ../agents/k194）
//
// 行清单（tools/soul_effects_dump.py 619，2026-10-07）：
//   619 傲世战皇 2907 args[30,0]「30%概率打出致命一击」（单子句，db[1]=0 无吸血）/
//     赛伯斯塔 2866 args[50,40]「50%概率打出致命一击;造成伤害的40%恢复自身体力」+
//     "伙伴效果"行 args[60,50]（同文 60%/50%）
//   → db[0]=暴击率%（百分比：50/30/60 tips 互锚）；db[1]=吸血%（40/50，0=无子句）。
//
// 实现（SOUL_PROGRAM 三节点）：
//   ① FIRST/SECOND_ACTION_START（J136 1527/1465 同款出手期 tick）：roundChoice 选技
//     && SkillType!=Attribute（§四点五选择期判据）→ roll db[0]% → ws.must_crit[own]。
//     ⚠️ 实测记档：暴击掷点在 query_usage ①.5（ON_SKILL_HIT），先于 EVENT_SKILL_USED
//     ——事件面写 must_crit 惠及不到本次攻击、ws 每回合 reset → crits 恒 0；
//     ACTION_START 域内置旗才是当次生效的正位。
//   ② db[1]>0 时再装 ATTACK DETECT 相位零状态票（K168 363 直系）：attacker==own &&
//     isRed && final>0 → heal_amount(final×db[1]/100)（DETECT 时点 final 已定含暴击、
//     hp 未扣；heal 吃封回血钳 max）。
//
// 三查记录（2026-10-07，立单时已核）：①内核 grep 619 无魂印行为硬编码；②effect_info
//   617/619 同号为技能空间模板（魂印以 effect_icon tips 为权威）；③soul_lib 并集无 619、
//   registry.soul 无 619——真缺口。
// 先例：K164 109（SKILL_USED 窗+roll）、k_round_recover 227（ws.must_crit 旗）、
//   K168 363（DETECT 相位伤害回血）。
// 场景回归：test/scenario/scenario_3275_crit_drain.cpp（T3275-T3278）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 宿主行参数（各文件自治同款）────────────────────────────────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── ENTER：暴击掷点窗 + 吸血 DETECT 票 ─────────────────────────────────────
EffectResult soulmark_619_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 619);
    const int drain_pct = db_at(row, 1, 0);


    // 吸血 DETECT 票（db[1]=0 无子句不装）
    if (drain_pct > 0 && g_core && g_core->heal_amount) {
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, drain_pct](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 619)) {
                    return;
                }
                (void)g_core->heal_amount(c, owner, d.final * drain_pct / 100);
            });
    }
    return EffectResult::kOk;
}

// ── 出手期：roundChoice 判攻击技 → 掷点置当次必暴 ──────────────────────────
// ⚠️ 暴击掷点在 query_usage ①.5（ON_SKILL_HIT），先于 SKILL_EFFECT/SKILL_USED——
// 事件面写 must_crit 只惠及下次攻击且 ws 每回合 reset（实测 crits=0 记档）。正位 =
// J136 1527/1465 同款：ACTION_START 域内 tick，roundChoice+SkillType 静态判攻击技
// （§四点五选择期判据）后掷点，本次攻击的暴击判定在其后消费。
EffectResult soulmark_619_action_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 619)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 属性技不掷（"攻击技能"门）
    }
    const HostRow row = host_row(ctx, args, 619);
    const int crit_pct = db_at(row, 0, 0);
    if (crit_pct > 0 && roll_soul_percent(*ctx, 619, 0, owner, owner, crit_pct)) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(619, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_619_arm),
                   SoulMarkNodeRef(State::BATTLE_FIRST_ACTION_START,
                                   &soulmark_619_action_start),
                   SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_START,
                                   &soulmark_619_action_start)});
