// J 组 J162 · 降槽反馈族 —— 740×10（2026-10-04，worktree ../agents/j162）
//
// 家族语义（effect_info 官方模板即权威）：
//   740（arg_count=1）"{0}回合内每回合自身的能力下降状态都会反馈到对手身上"
//
// 实现口径：
//   - InRounds({0}) ROUND_START tick（register_round_effect，武装回合 ROUND_START 已过
//     → 实际自 R2 起每回合镜像；属性技纯武装载体无伤害面，语义无损）：
//     对自身每个负槽（六槽扫）→ stat_drop(对手, 同槽, 同幅度)——『反馈』字面镜像，
//     免弱 STAT_DROP 查询天然走原语。逐回合重施会自然叠层（官方"每回合反馈"字面）。
//
// 载体切片（unity 库 json_each 精确匹配 10/10；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-04）：740 [回合]：{4}×2/{3}×4/{2}×2/… 
//   纯链 永黯月辉 28073 [4]（cat4 属性技 pri0——场景武装载体）。
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 740
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：register_round_effect（InRounds 折算，common.h 196）、stat_drop（CoreApi 77，
//   查免弱）、六槽扫描（J4 has_no_stat_boost 同款遍历）。
// 场景回归：test/scenario/scenario_2530_drop_mirror.cpp（2530 号段）。

#include "common.h"

namespace {

// ── 740 tick：把自身负槽镜像 stat_drop 到对手 ──
EffectResult j162_740_mirror_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_drop || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = 1 - self;
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (c->ability_levels[self][i] < 0) {
            (void)g_core->stat_drop(c, opp, i, -c->ability_levels[self][i], self);
        }
    }
    return EffectResult::kOk;
}

// ── 740 —— {0}回合内每回合自身的能力下降状态都会反馈到对手身上 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_740_drop_mirror(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 740, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 740, args,
                     "740 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     740, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 740, args, "需要 回合 > 0");
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 740, rounds, {owner},
                          &j162_740_mirror_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(740, effect_740_drop_mirror)
