// J 组 J110 · 自施加/异常回满族 —— 2196×11 / 770×13（2026-10-03，worktree ../agents/j110）
//
// 家族语义（effect_info 官方模板即权威）：
//   2196（arg_count=2）"{0}%令自身{1}"
//   770（arg_count=0）"若对手处于异常状态，则恢复自身全部体力"
//
// 实现口径：
//   - 2196 = apply_anomaly_checked(**self**, {1}, random_duration, self, {0})
//     （J3 申报制，目标=自身；自身免疫票照挡）。SKILL_EFFECT 直通。
//   - 770 = 状态门（对手任一异常，has_active_anomaly CoreApi 槽 0..43 枚举）→
//     heal_max_hp_fraction(1) 回满。SKILL_EFFECT 直通。
//
// 载体切片（unity 库 json_each 精确匹配 11/13=24；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   2196 [概率,异常码]：{100,X}×11 全 100 档（纯链 火箭头槌 37587 / 泰山压顶 37609
//     均 [100,7]——异常 7=疲惫，确定性档）
//   770 零参：分散（纯链 火灵重生咒 17657 物150 / 水枪突刺 31446 / 汲魂复苏 36205）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 2196/770 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：apply_anomaly_checked（J3 申报制全家）、状态门+heal_max_hp_fraction
//   （770 同构的 191 邻族与 J47 回满）。
// 场景回归：test/scenario/scenario_2040_self_anom_heal.cpp（2040 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 2196 —— {0}%令自身{1} ──
// int_args = [owner, target, chance, anomaly]（args_num=2）。
EffectResult effect_2196_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2196, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2196, args,
                     "2196 布局=[owner,target,概率,异常码]");
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     2196, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0, 2196, args, "需要 概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2196, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, self, anomaly, random_anomaly_duration(),
                                        self, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 770 —— 若对手处于异常状态，则恢复自身全部体力 ──
// args_num=0：bind_participants → [owner, target]。
EffectResult effect_770_foe_anomaly_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 770, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 770, args,
                     "770 布局=[owner,target]（零参 bind_participants）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     770, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->has_active_anomaly || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    for (int id = 0; id <= 43; ++id) {
        if (g_core->has_active_anomaly(ctx, opp, id)) {
            (void)heal_max_hp_fraction(ctx, owner, 1);   // 回满（钳满/封回血天然生效）
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;   // 对手无异常 → 门关
}

}  // namespace

SKILL_EFFECT(2196, effect_2196_self_anomaly)
SKILL_EFFECT(770, effect_770_foe_anomaly_heal)
