// J 组 J252 · 属性复制族 —— 56×8（2026-10-24，worktree ../agents/j252）
//
// 官方模板（effect_info 56）：「{0}回合内使自身的属性和对方的属性相同」，args_num=1
// = side_effect.arg_count(56)=1（双表一致）。arg = 持续回合数 N。
//
// 行清单（unity 库 json_each 精确匹配 8 载体，5 纯 3 混；2026-10-24 查库）：
//   20145 属性复制  [56]|[5]        cat4 pri0（纯链）
//   21770 万能纽扣  [56]|[5]        cat4 pri0 must_hit（纯链）
//   22617 幻化之术  [56]|[5]        cat4 pri0 must_hit（纯链）
//   36191 守望者    [56]|[5]        特攻 135（纯链）
//   36596 解析模组  [56]|[5]        物攻 130（纯链）
//   26850 机甲变形  [48,47,56]|[4,4,4]   cat4 must_hit（48/47 已实现 anomaly_channel/stat_dispel）
//   27904 丛林模仿  [48,56,677]|[5,5,5]  cat4 must_hit（48/677 已实现）
//   34257 星冕耀闪光 [56,451]|[3,100,1]  特攻 150（451 已实现 h_chance_anomaly）
//   切片自洽：56 均取 1 参（{0}∈{3,4,5}），零剩余。
//
// 落法（55 反转战斗时属性直系同族，batch_b_skills.cpp effect_55_reverse_element）：
//   伤害公式的系别读取源 = ws.view_elementalAttributes（克制倍数、785 微弱等同源）；
//   SKILL_EFFECT 时点立即拷一次（本回合伤害结算在 SKILL_EFFECT 之后，立刻可见）
//   + InRounds({0}) ROUND_START tick 逐回合续拷（每回合 sync_workspace_from_on_stage
//   重灌视图 → 窗口过期自然还原；对手换宠后按新对手当前视图续拷——"和对方的属性相同"
//   是持续状态）。
//   已知近似（55 同款，家族口径）：自身回合中段换宠 → perform_switch sync 重基视图，
//   当回合剩余时间的复制丢失；窗内后续回合 tick 照旧续拷（side 级状态语义）。
//
// 动工前置三查（2026-10-24 本 worktree 复核）：
//   ①内核 grep 56 无硬编码 case（src/ 无 SKILL_EFFECT/效果分支涉及）
//   ②effect_meta 无此表（DB 无该表；认证层 custom_effect_overrides 未声明 → 默认 InRounds）
//   ③moves_lib 并集无 SKILL_EFFECT(56) 注册、effect_unit_parser 无该模板句式
//     （「…属性和对方的属性相同」不在 parser 动词表）——真缺口。
//   registry（build/registry.json）8 载体均未收录；moves=1393。
// 先例：55（batch_b 同族窗式）、51（j_power_view3 续拷形）、2210（tianqi element_view
//   范式参考）；window 语义见 battleContext.h round_effect_start_round（InRounds 默认）。
// 场景回归：test/scenario/scenario_7030_element_copy.cpp（号段 7030-7039）。

#include "common.h"

namespace {

// ── 56 tick：己方视图 ← 对方当前视图（每回合 ROUND_START，sync 重基之后）──
EffectResult j252_56_copy_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 2; ++i) {
        ctx->ws.view_elementalAttributes[self][i] =
            ctx->ws.view_elementalAttributes[opp][i];
    }
    return EffectResult::kOk;
}

// ── 56 —— {0}回合内使自身的属性和对方的属性相同 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_56_copy_element(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 56, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 56, args,
                     "56 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     56, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(rounds > 0, 56, args, "需要 回合 > 0");
    // 武装回合立即生效一次（55 同款：当回合伤害结算读得到复制后的视图）；
    // 后续回合由 InRounds ROUND_START tick 续拷，窗口过期 sync 自然还原。
    j252_56_copy_tick(ctx, EffectArgs(std::vector<int>{self, opp}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 56, rounds,
                          {self, opp}, &j252_56_copy_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(56, effect_56_copy_element)
