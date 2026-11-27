// J 组 J115 · 条件先制族二 —— 4 宏/50 载体（2026-10-03，worktree ../agents/j115）
//
// 家族语义（effect_info 模板即权威）：
//   958  [2] 若对手处于{0}状态则先制+{1}
//   975  [3] {0}%的概率威力{1}倍，若未触发则下回合自身先制+{2}
//   1081 [0] 若对手处于能力提升状态则先制+1
//   1174 [0] 若自身处于能力提升状态则先制+2
//
// 实现口径：
//   - 958/1174/1081 = 选择期路由（skills.cpp selection_effects_ 表加行，2298/779 同款
//     ——条件先制族注册进用技期分支赶不上首回合先制判定，J66 勘误）。体=条件判 →
//     ws.preemptive_level[self] += N：
//     · 958 条件 = g_core->has_active_anomaly(对手, {0})（102 同款 CoreApi 槽，
//       状态码为官方异常码 0-9/全集 ≤43）；
//     · 1174/1081 条件 = ability_levels 自身/对手侧任一**正槽**（2298/779 直读同判据，
//       六槽含命中）。
//   - 975 = 用技期 roll（不进选择表）：roll_percent({0}) → 威力视图 view×{1}
//     （102 通道，view>0 才写=声明变威力）；未触发 → 下回合自身先制+{2} =
//     register_next_rounds_effect(**FIRST_MOVE_RIGHT**, rounds=1) +
//     effect_move_right_priority_bonus（J4 1726/1310 同款——⚠️ 勘误：先制加成必须挂
//     FIRST_MOVE_RIGHT 桶，handle_BattleFirstMoveRight 进入时 memset preemptive_level，
//     ROUND_START 桶（J105 629 现行通道）加的值在比较前必被清零、从未生效；
//     629 场景绿系载体 16327 基值先制 +2 跨回合泄漏掩盖——R 单候选，见晨报）。
//
// 载体切片（unity 库 json_each 精确匹配 13+13+12+12=50；side_effect.arg_count 顺序切分
// 全载体自洽，唯 32899 琉璃净火链上 686 双表不一致（R1 已知现象族），958 本体切片 [2,1]
// 无恙——脚本核算 2026-10-03）：
//   958  [状态码,先制]：{2,1}×5/{2,3}×3/{5,1}×2/{1,3}/{6,1}×2 纯链 鬼幽斩杀 32422 [6,1]、
//        火石投掷 35729 [2,1]
//   975  [概率,倍数,先制]：{15,3,2}×4/{20,2,2}×3/{50,2,2}×2/… 纯链 能源轰炸 30565
//        [15,3,2]、战魂破灭弑 31126 [20,3,2]、完美几何 32598 [10,5,2]
//   1174 []（零参，+2 写死文案）纯链 无光之火 32915
//   1081 []（零参，+1 写死文案）纯链 铁之幻光 33512
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 4 id 无硬编码 case
// ②effect_meta 无行 ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 先例：2298/779（条件先制选择期体，j_gaia_stance）、102（异常门威力视图，batch_b）、
//   629（NextRounds ROUND_START 先制 tick，j_cleanse_prio_reversal_pp）、
//   has_active_anomaly/roll_percent/register_next_rounds_effect（common.h/CoreApi 槽）。
// 场景回归：test/scenario/scenario_2090_cond_prio2.cpp（号段 2090-2099，一文件四块）。

#include "common.h"

namespace {

// ── 958 —— 若对手处于{0}状态则先制+{1}（选择期体，2298 同形）──
// int_args = [owner, target, 状态码, 先制]（args_num=2）。
EffectResult effect_958_anom_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 958, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 958, args,
                     "958 布局=[owner,target,状态码,先制]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    const int prio = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     958, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(code), 958, args,
                     "需要 状态码 ∈ [0,43]");
    EFFECT_ARG_GUARD(prio > 0, 958, args, "需要 先制 > 0");
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, code)) {
        return EffectResult::kOk;   // 条件门：对手不处于{0}状态
    }
    ctx->ws.preemptive_level[self] += prio;
    return EffectResult::kOk;
}

// ── 1174 —— 若自身处于能力提升状态则先制+2（零参；2298 镜像：2298 判"不处于"，
//             本条判"处于"；正槽判据=ability_levels 任一 >0，六槽含命中）──
EffectResult effect_1174_self_boost_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1174, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1174, args,
                     "1174 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1174, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            ctx->ws.preemptive_level[self] += 2;   // 自身有提升 → +2
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 1081 —— 若对手处于能力提升状态则先制+1（零参；779 孪生，+1）──
EffectResult effect_1081_opp_boost_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1081, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1081, args,
                     "1081 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = 1 - self;
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1081, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            ctx->ws.preemptive_level[self] += 1;   // 对手有提升 → +1
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 975 —— {0}%的概率威力{1}倍，若未触发则下回合自身先制+{2}（用技期 roll，
//             不进选择表；威力半=102 通道，未触发半=629 的 NextRounds 先制 tick）──
// int_args = [owner, target, 概率, 倍数, 先制]（args_num=3）。
EffectResult effect_975_roll_power_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 975, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 975, args,
                     "975 布局=[owner,target,概率,倍数,先制]");
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int mult = args.int_args[3];
    const int prio = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     975, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && mult > 1 && prio > 0, 975, args,
                     "需要 概率/先制 > 0 且 倍数 > 1");
    if (roll_skill_percent(*ctx, 975, 0, self, args.int_args[1], chance)) {
        const int view = ctx->ws.skill_power_view[self];
        if (view > 0) {
            ctx->ws.skill_power_view[self] = view * mult;   // ×{1}（写=声明变威力，102 通道）
        }
        return EffectResult::kOk;
    }
    // 未触发 → 下回合自身先制+{2}（NextRounds 不含当回合；**必须挂 FIRST_MOVE_RIGHT
    // 桶**——进入时 memset preemptive_level，ROUND_START 挂的加成活不到比较，
    // effect_move_right_priority_bonus 载荷=[owner,target,bonus]，J4 1726 同款）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 975,
                                /*rounds=*/1, {self, 1 - self, prio},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(958, effect_958_anom_prio)
SKILL_EFFECT(975, effect_975_roll_power_prio)
SKILL_EFFECT(1081, effect_1081_opp_boost_prio)
SKILL_EFFECT(1174, effect_1174_self_boost_prio)
