// J 组 J142 · 连击面状态计数族 —— 1 宏/14 载体（2026-10-04，worktree ../agents/j142）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1108 [自身步长,对手步长] "自身每处于一种能力提升状态则技能连击数+{0}，
//     对手每处于一种弱化状态则技能连击数+{1}"
//     —— 连击面家族直系（j_combo_face.cpp 五先例）：SKILL_EFFECT 时点**数种类**
//     ——自身六槽正槽种类数×{0} + 对手六槽负槽种类数×{1}（J13/J21 判据：六槽含
//     命中、按种类计不按等级和）→ 总段数 = 1 + N（"连击数+N"：无状态=普通单段，
//     模板字面；4399 主载体未收录，按字面记档）→ 挂 ws.combo_count_resolver
//     （materialize 咨询点语义 29180df：finish_attack_damage 变威力重算前咨询一次、
//     写回 combo_view，用后即焚）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 14/14 全=[1,1] 自洽、零剩余）：
//   纯链：深海多重击 33483 [1,1]（特25——场景主载体；其余载体同链未实现多处，
//   合并期激活风险，场景禁用——工单口径）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 1108 无硬编码 ②battle_effects
//   无行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：j_combo_face.cpp 612/1685（resolver 挂法：lambda 返回总段数、一次性）、
//   J13/J21（正/负槽种类计数判据）。
// 场景回归：test/scenario/scenario_2400_combo_state_count.cpp（号段 2400-2409，
//   hitCount 确定值断言，不吃伤害浮动）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 六槽种类计数（batch_c count_boost_kinds_c 同判据：0=正槽 1=负槽，含命中）。
int j142_kind_count(const BattleContext* ctx, int owner, bool negative) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (negative ? ctx->ability_levels[owner][i] < 0
                     : ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 1108 —— 自身提升种类×{0} + 对手弱化种类×{1} → 连击数+N ──
// int_args = [owner, target, {0}自身步长, {1}对手步长]（args_num=2）。
EffectResult effect_1108_state_count_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1108, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1108, args,
                     "1108 布局=[owner,target,自身步长,对手步长]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int step_self = args.int_args[2];
    const int step_opp = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1108,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(step_self > 0 && step_opp > 0, 1108, args, "需要 步长 > 0");
    // SKILL_EFFECT 时点数种类（工单口径）；总段数 = 1 + N（模板"连击数+N"字面）。
    const int extra = j142_kind_count(ctx, owner, /*negative=*/false) * step_self
                    + j142_kind_count(ctx, opp, /*negative=*/true) * step_opp;
    ctx->ws.combo_count_resolver[owner] = [total = 1 + extra](BattleContext*) {
        return total;
    };
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1108, effect_1108_state_count_combo);
