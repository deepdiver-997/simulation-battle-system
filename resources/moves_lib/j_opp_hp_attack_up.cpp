// J 组 J201 · 体力门攻击递增族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   487 [2] 若对手的体力大于{0}，则每次使用自身攻击+{1}
//
// 实现口径：
//   - SKILL_EFFECT 时点：读对手**当前体力**（严格 > {0}；等于阈值不触发）→
//     stat_change(self, 攻槽 0, +{1})——自增益走真原语（本体/视图同步；不吃免弱）。
//   - "每次使用"= 每次技能执行到效果分支各结算一次（本技能连续使用逐次累加）。
//   - 无审计槽（无随机；等级槽本身即观测面）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   487 [体力阈值,提升点]：36882 渐落之芳 纯链 [350,2]（特 140，场景载体）、
//        33759 [300,1] 等（9 载体，{1} 为 1~2）
//
// 动工前置三查（2026-10-22）：①内核 grep 487 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(487)——真缺口。
// 场景回归：test/scenario/scenario_5260_opp_hp_attack_up.cpp（号段 5260-5269）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 487 —— 若对手的体力大于{0}，则每次使用自身攻击+{1} ──
// int_args = [owner, target, 体力阈值, 提升点]（args_num=2）。
EffectResult effect_487_opp_hp_attack_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 487, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 487, args,
                     "487 布局=[owner,target,体力阈值,提升点]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     487, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && delta > 0, 487, args, "需要 阈值/提升点 > 0");
    if (ctx->getPet(opp).hp <= threshold) {
        return EffectResult::kOk;   // 门：对手体力严格大于阈值
    }
    if (g_core && g_core->stat_change) {
        (void)g_core->stat_change(ctx, self, /*攻击槽=*/0, delta);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(487, effect_487_opp_hp_attack_up)
