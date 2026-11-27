#include <utils/sbs_rand.h>
// J 组 J173 · 随机恢复族 —— 1132×12 / 70×13（2026-10-04，worktree ../agents/j173）
//
// 家族语义（effect_info 官方模板即权威）：
//   1132 [4] 随机恢复自身{0}-{1}点体力，自身不处于能力提升状态时变为{2}-{3}点体力
//   70   [0] 威力随机，随机范围150~220
//
// 实现口径：
//   - 1132 = SKILL_EFFECT：任一正槽（count_boost 判据，J51 同款六槽扫）→
//     rand[{0},{1}]，否则 rand[{2},{3}]（模板语序：默认 {0}-{1}，无提升"变为"
//     {2}-{3}——载体切片 {2}-{3}>{0}-{1}，无提升补偿向）→ heal_amount（点数口径）。
//   - 70 = J159 118 直系零参版：skill_power_view = 150 + rng.next()%71（常数写死文案，
//     "威力域随机"消耗一次 rand，非概率事件）。
//
// 载体切片（unity 库 json_each 精确匹配 12+13=25；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   1132 [lo1,hi1,lo2,hi2]：{200,400,300,500}/{150,300,200,350}/… 纯链×4
//     （飞仙灵龙诀 34609 [150,300,200,350] pri0——场景载体）
//   70   []（零参）纯链×13（远古审判 10847 cat2 pri0——场景载体）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：1144/604（rand 区间，j_anom_crit_rand/e_power_crit_ramp）、118（J159 绝对写）、
//   heal_amount（CoreApi 68）、count_boost 六槽扫（J51）。
// 场景回归：test/scenario/scenario_2750_rand_heal_power.cpp（2750 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 1132 —— 随机恢复；无提升切区间 ──
// int_args = [owner, target, lo1, hi1, lo2, hi2]（args_num=4）。
EffectResult effect_1132_rand_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1132, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1132, args,
                     "1132 布局=[owner,target,lo1,hi1,lo2,hi2]");
    const int owner = args.int_args[0];
    const int lo1 = args.int_args[2];
    const int hi1 = args.int_args[3];
    const int lo2 = args.int_args[4];
    const int hi2 = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1132, args, "需要 owner ∈ {0,1}");
    // 下限允许 0（"恢复0-500点"合法——恢复 0 = 空操作；fuzz 0705 批次实测）：
    EFFECT_ARG_GUARD(lo1 >= 0 && hi1 >= lo1 && lo2 >= 0 && hi2 >= lo2, 1132, args,
                     "需要 0 ≤ 下限 ≤ 上限");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    bool boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            boosted = true;
            break;
        }
    }
    const int lo = boosted ? lo1 : lo2;
    const int hi = boosted ? hi1 : hi2;
    const int amount = lo + ctx->rng().next() % (hi - lo + 1);
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

// ── 70 —— 威力随机，随机范围150~220（零参；118 直系零参版）──
EffectResult effect_70_random_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 70, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 70, args,
                     "70 布局=[owner,target]（on_selected 绑定）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 70, args, "需要 owner ∈ {0,1}");
    ctx->ws.skill_power_view[owner] = 150 + ctx->rng().next() % 71;   // 150..220 绝对置
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1132, effect_1132_rand_heal)
SKILL_EFFECT(70, effect_70_random_power)
