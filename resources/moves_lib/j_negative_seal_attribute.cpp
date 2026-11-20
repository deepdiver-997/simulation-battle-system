// J 组 J248 · 降态封属窗族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1438（args_num=1）"若对手处于能力下降状态，则{0}回合内令对手使用的属性技能无效"
//
// 实现口径：
//   - 门 = 对手 6 个能力槽的视图等级**任一为负**（"处于能力下降状态"；读
//     `ws.view_levels`，与伤害公式同源）。
//   - 落点 = `seal_skill(source=owner, target=对手, attribute=true, attack=false,
//     count=0, duration_rounds={0}, penetrable=true, ON_STAGE, chance=100)`
//     —— 478/1923 同通道（属性技封，回合窗口型；"无效"走技能无效补偿口径）。
//   - `{0}` 在位即消费：<=0 = 不封（不回退默认值）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   28305 [970,1338,1438]|[3,1,2] / 29175 [191,1348,1438]|[4,3,2,2] /
//   33600 [1164,1438,1050]|[2,2,50,15] /
//   **35891 龙舞·逆 [1009,1438]|[1,2]（1009 清对手正等级——对本场景预置的负态无操作）** /
//   36120 [1052,1122,1438]|[1,1,8,2] / 37426 [1318,1438,1497]|[5,1,1] /
//   38113 [1684,1478,1438]|[2,1,2] / 38400 [1726,1438]|[2,1,1,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1438 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5790_negative_seal_attribute.cpp（号段 5790-5799）。

#include "common.h"

namespace {

// ── 1438 —— 对手处于能力下降状态 → {0}回合内属性技能无效 ──
EffectResult effect_1438_negative_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1438, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1438, args,
                     "1438 布局=[owner,target,rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1438,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不封（不回退默认值）。
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    bool has_drop = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ws.view_levels[opp][i] < 0) {
            has_drop = true;
            break;
        }
    }
    if (!has_drop) {
        return EffectResult::kOk;   // 对手不处于能力下降状态
    }
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1438,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1438, effect_1438_negative_seal_attribute)
