// D10 · 不灭地威·萨瑞卡 3637 第五技「X·天罡碎岩 32178」缺失效果 774、1086
//（夜跑 night-2，2026-09-28）
//
// 32178 链 = [485, 774, 1086]、args [3, 50]：
//   485=[]（消强成功回满血，effect_batch8.cpp 已接）、774=[3]、1086=[50]（本文件）。
// 4399 图鉴（X·天罡碎岩）："必中；消除对手能力提升状态，消除成功则恢复自身所有体力；
//   自身体力高于对手则回合结束时减少对手1/3最大体力，自身体力低于对手时造成的伤害提高50%"。
//
// ⚠️ 时点口径记档：effect_info 模板写"附加对手最大体力1/{0}的百分比伤害"（效果随链
//    即时结算），4399 图鉴转述为"回合结束时减少"。按模板取 SKILL_EFFECT 即时结算
//    （1853"附加对手最大体力1/N的百分比伤害"同短语同落点；args 切片 1/3 与图鉴一致）。
//    若实测需挪回合结束时点，只改注册时点，数值式不动。
//
// 774 "若自身当前体力高于对手则附加对手最大体力1/{0}的百分比伤害"：
//   - 严格高于（hp_self > hp_opp，相等不发）；
//   - 量 = max_hp_of(target)/denom（PERCENT_VALUE：值已算好、走百分比抗性/护罩）。
// 1086 "自身体力低于对手时造成的伤害提高{0}%"：
//   - 严格低于；增伤 = ws.damage_add_pct += {0}（AMP 加法通道，1178/1959 同款）。
//   - 与 774 按体力互斥成对（高于/低于），相等回合两者皆不发。
//
// 场景回归：test/scenario/scenario_260_sarika_3637_774_1086.cpp。

#include "common.h"

namespace {

// ── 774 —— 自身当前体力高于对手 → 附加对手最大体力 1/{0} 的百分比伤害 ──
EffectResult effect_774_hp_lead_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= ctx->getPet(target).hp) {
        return EffectResult::kOk;   // 未严格高于对手：不触发
    }
    const int amount = max_hp_of(ctx, target) / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 1086 —— 自身体力低于对手时造成的伤害提高{0}% ──
EffectResult effect_1086_hp_behind_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp >= ctx->getPet(target).hp) {
        return EffectResult::kOk;   // 未严格低于对手：不增伤
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(774, effect_774_hp_lead_percent);
SKILL_EFFECT(1086, effect_1086_hp_behind_amp);
