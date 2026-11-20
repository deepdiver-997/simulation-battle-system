// J124 · 条件输出族二 —— 890×12 / 488×13 / 987×12（2026-10-03，worktree ../agents/j124）
//
// 家族语义（effect_info 官方模板 + 载体切片核对），三条都是"条件 → 输出放大/附加"：
//
// 890（args_num=0）"自身处于能力提升状态时造成的伤害翻倍"：
//   - 条件 = 任一**正槽**（六槽含命中，e_priority has_any_positive_boost 同判据，
//     就地重写——匿名空间不跨 TU）；
//   - "翻倍" = ws.damage_add_pct[自身] += 100（776/500 通用增伤加算通道，
//     落本次攻击 snapshot）。
// 488（args_num=2）"若对手的HP小于{0}，则造成的伤害增加{1}%"：
//   - 1439 单刀条件 amp 同构：SKILL_EFFECT **施放期求值**（模板无"期间"字样）——
//     对手当前 hp（getPet().hp 引用别名）严格 < {0} → damage_add_pct += {1}。
// 987（args_num=2）"附加{0}点固定伤害，对手{1}时固定伤害翻倍"：
//   - J23 500/36 直系：fixed_damage(对手, {0}) 无条件附加（模板无命中门）；
//     翻倍判据 has_active_anomaly(对手, {1})（与施加通道 apply_anomaly 配对的查询
//     原语，J23 同款）；**倍在申报前折算**（一次 fixed_damage 调用带 ×2 量，
//     不做两次调用——工单行口径）。
//
// 动工前置三查（2026-10-03）：①内核 grep 890/488/987 无硬编码 ②effect_meta 无分类
//   牵连 ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect.arg_count
//   890=0/488=2/987=2 与 effect_info.args_num 一致；37 载体 []/[阈值,pct]/
//   [固伤,异常码] 自洽零剩余（脚本核算 2026-10-03）。
// 先例：e_priority（正槽判据）、J 组 1439（单刀条件 amp，j_first_second_gates）、
//   J23 97/500（翻倍通道+异常查询）、J2 1170（固伤附加）。
// 场景回归：test/scenario/scenario_2180_cond_output2.cpp（2180 号段，一文件三宏）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 自身任一正槽（六槽含命中；e_priority/batch 系同判据，就地重写）。
bool j124_has_any_positive_boost(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 890 —— 自身处于能力提升状态时造成的伤害翻倍（零参）──
// int_args = [owner, target]（args_num=0，bind_participants 仍带 [owner,target]）。
EffectResult effect_890_boost_damage_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 890, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 890, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     890, args, "owner/target 越界");
    if (j124_has_any_positive_boost(ctx, self)) {
        ctx->ws.damage_add_pct[self] += 100.0f;   // "翻倍"= 通用增伤 +100%（776 同通道）
    }
    return EffectResult::kOk;
}

// ── 488 —— 若对手的HP小于{0}，则造成的伤害增加{1}% ──
// int_args = [owner, target, hp_threshold, pct]（args_num=2）。
EffectResult effect_488_lowhp_cond_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 488, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 488, args,
                     "488 布局=[owner,target,hp阈值,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int hp_threshold = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 488, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(hp_threshold > 0 && pct > 0, 488, args, "需要 hp阈值/pct > 0");
    if (ctx->getPet(target).hp < hp_threshold) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 施放期求值，落本次攻击
    }
    return EffectResult::kOk;
}

// ── 987 —— 附加{0}点固定伤害，对手{1}时固定伤害翻倍 ──
// int_args = [owner, target, fixed, anomaly]（args_num=2）。
EffectResult effect_987_append_fixed_anomaly_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 987, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 987, args,
                     "987 布局=[owner,target,固伤,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 987, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 987, args, "需要 固伤 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 987, args, "异常码越界（≤43）");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int amount = fixed;
    if (g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, anomaly)) {
        amount *= 2;   // 倍在申报前折算（工单口径：一次调用带 ×2 量）
    }
    (void)g_core->fixed_damage(ctx, opp, amount);
    return EffectResult::kOk;
}

SKILL_EFFECT(890, effect_890_boost_damage_double)
SKILL_EFFECT(488, effect_488_lowhp_cond_amp)
SKILL_EFFECT(987, effect_987_append_fixed_anomaly_double)
