// J 组 J221 · 下降双倍反馈+免弱窗族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1260 [1] 将自身能力下降状态双倍反馈给对手，反馈成功则{0}回合内免疫能力下降效果
//
// 实现口径（直系先例）：
//   - 反馈半 = j_stat_drop_reflect.cpp 813：自身每个负槽（六槽含命中）按 2×|深度|
//     走 stat_drop(对手, 同槽, ...)——弱化原语查免弱（STAT_DROP）与附加禁令、钳 -6；
//     actor=属主如实声明。自身下降不清除（模板无清除子句，只"反馈"）。
//   - 反馈成功 = ≥1 槽 stat_drop 落地 SUCCESS（1030 同判据；对手免弱/全在 -6 →
//     反馈未落地 → 不授免弱票）。
//   - 免疫半 = canglan_nutao_skills.cpp 1194 同款：反馈成功 →
//     grant_immunity(STAT_DROP, coverage_all, rounds, source_effect_id=1260)（窗口票，
//     counts=0 非次数型；覆盖键带 effect_id，不与其它来源的免弱票互相覆盖）。
//   - 行参在位即消费：{0} 直读，不设默认窗长；显式 0/负档 = 不授票（反馈照常落地）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 10/10 自洽）：1260 [rounds]：
//   27510 异电鬼面 [3]（scenario 载体：cat4 属性技无伤害面；链兄弟 48 免异常/776 攻击翻倍）
//   27567 [4] / 27784 [3] / 28293 [4] / 31288 [3] / 31575 [3] / 31985 [3] /
//   32298 [3] / 33010 [2] / 33926 [4]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1260 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5500_drop_reflect_immune.cpp（号段 5500-5509）。

#include "common.h"

namespace {

// ── 1260 —— 下降双倍反馈；反馈成功则{0}回合内免能力下降 ──
// int_args = [owner, target, rounds]（args_num=1 → 装载器注入 owner/target）。
EffectResult effect_1260_reflect_double_then_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1260, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1260, args,
                     "1260 布局=[owner,target,rounds]（args_num=1）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1260,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 反馈半：逐负槽双倍深度镜像（SUCCESS 含"压到 -6 为止"的部分生效）。
    bool landed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int depth = ctx->ability_levels[owner][i];
        if (depth >= 0) {
            continue;
        }
        if (g_core->stat_drop(ctx, opp, i, -2 * depth, owner) == StatDropResult::SUCCESS) {
            landed = true;
        }
    }
    if (!landed) {
        return EffectResult::kOk;   // 反馈未落地 → 后缀不申报
    }
    // 免疫半：{0} 在位即消费——0/负档 = 不授票（不回退默认窗长）。
    if (rounds > 0) {
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                            /*anomaly_mask=*/0, rounds, /*source_id=*/0,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/1260);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1260, effect_1260_reflect_double_then_immune)
