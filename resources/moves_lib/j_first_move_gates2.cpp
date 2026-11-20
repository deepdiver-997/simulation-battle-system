#include <utils/sbs_rand.h>
// J 组 J114 · 先出手门族二 —— 750×15 / 1508×15 / 680×14 / 1467×13 / 528×13 / 1254×12
// + 790×19（J42 记档残留，同机制顺带补齐）（2026-10-03，worktree ../agents/j114）
//
// 家族语义（effect_info 官方模板即权威）：
//   750 （0 参）"先出手时无视伤害限制效果"
//   1508（0 参）"先出手时无视攻击免疫效果"
//   790 （1 参）"{0}回合内自身所有攻击无视伤害限制效果"
//   680 （3 参）"先出手时{0}%使对手{1}{2}回合"
//   1467（2 参）"随机使自身{0}项能力值+{1}，先出手时提升效果翻倍"
//   528 （1 参）"先出手时{0}回合内不受能力下降技能影响"
//   1254（0 参）"先出手时消除对手回合类效果"
//
// 实现口径：
//   - 先/后手门 = acting_side_is_first(ctx)（common.h，FIRST_* 状态域；SKILL_EFFECT 桶
//     被镜像到 FIRST/SECOND 两侧，currentState 即出手侧——J17/H2 同判据）。
//   - ★ 750/1508 穿透凭证的时点攻坚（本单核心，内核一行放行 + 路由体两级注册）：
//     · 凭证消费点在 query_usage（materialize_attack_credential → 盔 notify / CAP 回调），
//       早于 SKILL_EFFECT —— 执行期授予救不了当前一刀；
//     · 选择期 MOVE_RIGHT 桶又在**先手权比较之前**跑（双方共用 BATTLE_FIRST_MOVE_RIGHT
//       状态、桶内 acting_side_is_first 恒真）——此处也判不了"先出手"；
//     · 解法：路由体在 MOVE_RIGHT 注册 **BATTLE_FIRST_ACTION_START 一次性 tick**
//       （rounds=1、owner=self）——该桶在先手方行动开始时、query_usage 之前执行，
//       tick 内 acting_side_is_first 已可信（FIRST 域）→ grant_penetration 当回合窗口
//       （rounds=1、PENETRATE_ATTACK 门）。后手方时间线不跑 FIRST_ACTION_START 桶
//       （桶按 robotId 过滤），tick 不执行；窗口回合末自然过期。
//     · 内核 skills.cpp 配套一行：Penetration 分类此前一律 continue 吞并（pf 全零时
//       既无凭证又吞掉注册）——改为只吞并静态可合并的 697/699，其余"无视…"句式放行
//       （J42 790 记档"效果函数未被调用"的根因，本单修复）。
//   - 790 = grant_penetration 窗口票（rounds={0}，790=J42 头注设计原样落地）+ **双写**
//     ws.attack_credential[self].ignore_damage_limit 兜当前刀（当前刀凭证已在
//     query_usage 物化，窗口票下刀才可见；CAP 回调在管线内读 ws 槽，双写即时生效）。
//   - 680 = 173 直系（H2 先出手概率异常）：先短路后掷骰纪律 + apply_anomaly_checked
//     申报制（chance={0}、duration={2}、ChanceSource::Skill）。
//   - 528 = 471 双胞胎（H2 头注"异常半句=471、能力下降半句=528"）：grant_immunity
//     (STAT_DROP, coverage_all, duration={0})——1194/STAT_DROP 票形（canglan 直系）；
//     stat_drop 原语第一件事查 STAT_DROP，免疫则 BLOCKED（battle_primitives.h:658）。
//   - 1254 = 1034/1200 同原语：break_round_effects(ctx, opp)（查免断 + 发 EVENT_BREAK +
//     清回合类盔威封属；无"消除成功"后缀子句，返回值不分支）。
//   - 1467 = 583 随机不重复挑槽（effect_batch8）+ stat_change 真原语（正槽增益不吃免弱
//     ——增益不走申报）；翻倍在挑槽前折算 delta（{1}×2 / {1}）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 逐条脚本核算 2026-10-03）：
//   750 全 0 参：17201 混沌利爪、19011 激光刀（pri=3）、30794 芳影难觅…
//   1508 全 0 参：37733 超电弧（纯链）、33922 白昼流星 [1508,857]…
//   790  [回合]：26434 纯链 [3]、24164 真龙霸体 [191,576,790]|[…,3]
//   680  [概率,异常码,回合]：37262 悬冰伏徒 [100,15,2]（100% 确定档场景载体）、
//        30414 电光逆枪舞 [60,0,3]、19502 残念一刀 [40,15,3]…
//   1467 [项数,增量]：27989 衍生怨灵（纯链属）、33295 神圣一击 [1062,1467]|[3,3,1]…
//   528  [回合]：31196 自动攻击（纯链）、33429 未来·闪击 [766,471,528]|[2,200,2,2]（H2 互证）
//   1254 全 0 参（全链上伙伴）：31260 千刃影袭破 [1194,1254]、31924 暗月绞杀 [1254,648]…
//
// 动工前置三查（2026-10-03）：①内核 grep 7 id 无行为硬编码（唯 skills.cpp 吞并分支，
// 本单修复）②effect_meta：750/1508/790 被关键词分类为 Penetration（即吞并根因），
// 680/528/1254/1467 分类 Other 正常派发 ③moves_lib 无既有注册——全族真缺口。
// ⚠️ 切片残留 1 条：19789 烬燃死灰箭 [680,954]|[60,6,3,1]——954 arg_count=0，
//    尾参 [1] 无主（R1 台账口径），该载体不进场景。
// 场景回归：test/scenario/scenario_2080_piercing_gates.cpp / scenario_2081_first_cond.cpp
//           / scenario_2082_break_pierce_window.cpp（号段 2080-2089）。

#include "common.h"

namespace {

// ── 750/1508 的 FIRST_ACTION_START 一次性 tick ──
// 先手方行动开始（query_usage 之前）判 acting_side_is_first（FIRST 域恒真，防御式保留）
// → 授予当回合穿透窗口（PENETRATE_ATTACK 门；query_usage 物化时读，当刀即生效）。
EffectResult j114_tick_first_penetrate_750(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_penetration(owner, /*level=*/1, /*ignore_attack_immunity=*/false,
                           /*ignore_damage_limit=*/true, /*counts=*/0, /*rounds=*/1,
                           RuleCategory::PENETRATE_ATTACK, EffectScope::ON_STAGE,
                           /*source_effect_id=*/750);
    return EffectResult::kOk;
}

EffectResult j114_tick_first_penetrate_1508(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_penetration(owner, /*level=*/1, /*ignore_attack_immunity=*/true,
                           /*ignore_damage_limit=*/false, /*counts=*/0, /*rounds=*/1,
                           RuleCategory::PENETRATE_ATTACK, EffectScope::ON_STAGE,
                           /*source_effect_id=*/1508);
    return EffectResult::kOk;
}

// 路由体（MOVE_RIGHT 桶，先手权比较前）：把授予推迟到先手方 ACTION_START 一次性 tick。
void j114_register_first_move_tick(BattleContext* ctx, int owner, int effect_id,
                                   EffectFn tick) {
    (void)register_round_effect(ctx, State::BATTLE_FIRST_ACTION_START, owner, effect_id,
                                /*rounds=*/1, std::vector<int>{owner}, tick);
}

// ── 750 —— 先出手时无视伤害限制效果 ──
// int_args = [owner, target]（0 参效果，装载器注入）。选择期路由（skills.cpp 2026-10-03）。
EffectResult effect_750_first_ignore_damage_limit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 750, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 750, args,
                     "750 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 750, args, "需要 owner ∈ {0,1}");
    j114_register_first_move_tick(ctx, owner, 750, &j114_tick_first_penetrate_750);
    return EffectResult::kOk;
}

// ── 1508 —— 先出手时无视攻击免疫效果 ──
EffectResult effect_1508_first_ignore_attack_immunity(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1508, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1508, args,
                     "1508 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1508, args, "需要 owner ∈ {0,1}");
    j114_register_first_move_tick(ctx, owner, 1508, &j114_tick_first_penetrate_1508);
    return EffectResult::kOk;
}

// ── 790 —— {0}回合内自身所有攻击无视伤害限制效果 ──
// int_args = [owner, target, 回合]（args_num=1）。窗口票 + 双写当前刀（J42 头注设计）。
EffectResult effect_790_window_ignore_damage_limit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 790, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 790, args,
                     "790 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0, 790, args, "需要 owner∈{0,1} 回合>0");
    ctx->grant_penetration(owner, /*level=*/1, /*ignore_attack_immunity=*/false,
                           /*ignore_damage_limit=*/true, /*counts=*/0, /*rounds=*/rounds,
                           RuleCategory::PENETRATE_ATTACK, EffectScope::ON_STAGE,
                           /*source_effect_id=*/790);
    // 当前刀凭证已在 query_usage 物化（早于本 SKILL_EFFECT）→ 直写 ws 槽兜当前刀
    //（ws 每回合 reset、下刀 materialize 重读 RuleCenter 窗口票，双保险即时+窗口）。
    ctx->ws.attack_credential[owner].ignore_damage_limit = true;
    return EffectResult::kOk;
}

// ── 680 —— 先出手时{0}%使对手{1}{2}回合 ──
// int_args = [owner, target, 概率, 异常码, 回合]（args_num=3）。173 直系：先短路后掷骰。
EffectResult effect_680_first_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 680, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 680, args,
                     "680 布局=[owner,target,概率,异常码,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     680, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && rounds > 0, 680, args, "需要 概率/回合 > 0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门：后手无事发生（不耗 rand）
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, rounds, owner, chance,
                                        ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1467 —— 随机使自身{0}项能力值+{1}，先出手时提升效果翻倍 ──
// int_args = [owner, target, 项数, 增量]（args_num=2）。583 随机不重复挑槽同款。
EffectResult effect_1467_random_boost_first_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1467, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1467, args,
                     "1467 布局=[owner,target,项数,增量]");
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && count > 0 && delta > 0, 1467, args,
                     "需要 owner∈{0,1} 项数/增量>0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int step = acting_side_is_first(ctx) ? delta * 2 : delta;   // 先手翻倍
    std::vector<int> slots;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        slots.push_back(i);
    }
    for (int k = 0; k < count && !slots.empty(); ++k) {
        const int pick = sbs_rand() % static_cast<int>(slots.size());
        (void)g_core->stat_change(ctx, owner, slots[static_cast<std::size_t>(pick)], step);
        slots.erase(slots.begin() + pick);
    }
    return EffectResult::kOk;
}

// ── 528 —— 先出手时{0}回合内不受能力下降技能影响 ──
// int_args = [owner, target, 回合]（args_num=1）。471 双胞胎：STAT_DROP 票（1194 票形）。
EffectResult effect_528_first_stat_drop_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 528, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 528, args,
                     "528 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0, 528, args,
                     "需要 owner∈{0,1} 回合>0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                        rounds, /*source_id=*/0, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/528);
    return EffectResult::kOk;
}

// ── 1254 —— 先出手时消除对手回合类效果 ──
// int_args = [owner, target]（0 参效果）。1034/1200 同原语（免断/事件在原语内处理）。
EffectResult effect_1254_first_break_rounds(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1254, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1254, args,
                     "1254 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1254, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, opp);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(750, effect_750_first_ignore_damage_limit)
SKILL_EFFECT(1508, effect_1508_first_ignore_attack_immunity)
SKILL_EFFECT(790, effect_790_window_ignore_damage_limit)
SKILL_EFFECT(680, effect_680_first_anomaly)
SKILL_EFFECT(1467, effect_1467_random_boost_first_double)
SKILL_EFFECT(528, effect_528_first_stat_drop_immune)
SKILL_EFFECT(1254, effect_1254_first_break_rounds)
