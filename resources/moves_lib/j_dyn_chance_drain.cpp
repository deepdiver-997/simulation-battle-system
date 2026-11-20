// J 组 J93 · 动态概率/后手吸取族 —— 1365×15 / 1189×14（2026-10-03，worktree ../agents/j93）
//
// 家族语义（effect_info 官方模板即权威）：
//   1365（arg_count=4）"命中后{0}%令对手{1}，自身每损失{2}%的体力概率提升{3}%"
//   1189（arg_count=1）"后出手时吸取对手{0}点体力"
//
// 实现口径：
//   - 1365 = 申报制动态 chance（J3 申报制 + J57 1401 同构）：生效概率 =
//     {0} + floor(已损体力百分比 / {2}) × {3}（已损 = (max-hp)×100/max，整数分步、
//     上限 100——clamp 防越界）；apply_anomaly_checked 概率申报给原语（亮节闸门
//     管辖），random_anomaly_duration。
//   - 1189 = 后手门（acting_side_is_first 取反，J57 1092 同判据）+ drain_points
//     定点吸取（J68/J4 通道：fixed_damage 落点 + 按 dealt 回吸）。模板无门槛
//     （"后出手时"直吸）。
//
// 载体切片（unity 库 json_each 精确匹配 15/14=29；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1365 [基础概率,异常码,步长,增量]：{80,6,20,5}/{80,7,20,5}/{50,8,20,10}/…
//     （场景载体 狂战荡风锤 35860 [867✓,1365,1366✗]|[2,80,6,20,5,…] 物160——
//     867 已验良性、1366 哑；base 80 + 满步 25 = 105 → clamp 100 确定性档）
//   1189 [点数]：{300}×9/{250}×2/{200}×2/{150}
//     （纯链载体 复苏之火 31082 [150] 特130）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1365/1189 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1401/1092（申报制+后手族，J57）、36（概率申报），drain_points（J68/J4）。
// 场景回归：test/scenario/scenario_1820_dyn_chance_drain.cpp（1820 号段，一文件两宏）。

#include "common.h"

namespace {

// 定点吸取（J68/J4 同款本文件自备份）。
void j93_drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount || !ctx) {
        return;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
}

// ── 1365 —— 命中后{0}%令对手{1}，自身每损失{2}%的体力概率提升{3}% ──
// int_args = [owner, target, base_chance, anomaly, step, up]（args_num=4）。
EffectResult effect_1365_dyn_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1365, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1365, args,
                     "1365 布局=[owner,target,基础概率,异常码,步长,增量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base_chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int step = args.int_args[4];
    const int up = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1365, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base_chance > 0 && step > 0, 1365, args, "需要 基础概率/步长 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1365, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 动态概率：已损百分比（对 max_hp），每 {2}% 一档 +{3}，上限 100。
    const int max_hp = max_hp_of(ctx, owner);
    const int lost_pct = (max_hp - ctx->getPet(owner).hp) * 100 / max_hp;
    const int chance =
        std::min(100, base_chance + (step > 0 ? lost_pct / step * up : 0));
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1189 —— 后出手时吸取对手{0}点体力 ──
// int_args = [owner, target, points]（args_num=1）。
EffectResult effect_1189_second_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1189, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1189, args,
                     "1189 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1189, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1189, args, "需要 点数 > 0");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    j93_drain_points(ctx, owner, opp, points);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1365, effect_1365_dyn_chance_anomaly)
SKILL_EFFECT(1189, effect_1189_second_drain)
