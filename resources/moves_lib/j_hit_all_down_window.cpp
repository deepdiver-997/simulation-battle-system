// J 组 J243 · 命中全属性降窗族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   965（args_num=2）"{0}回合内自身攻击技能命中则对手全属性-{1}"
//
// 实现口径（窗口形，470 直系同款）：
//   - 窗 = InRounds({0})，锚点 = 本次技能使用（当回合起算；register_round_effect
//     走 round_effect_start_round 折算，先后手都正确）。
//   - 触发面 = FIRST/SECOND_ON_SKILL_HIT 双侧桶（470 同款）：该时点桶只在**未 invalid**
//     （非 miss / 非被盔 / 非封属）时执行；回调再自卡
//     `executed_skill_is_attack(owner)`（属性技不触发）+ `skill_exec_result==HIT`。
//   - 落点 = 对手 6 槽逐个 `stat_drop(对手, i, {1})`（"全属性"含命中共 6 槽，
//     749/938 同款；走弱化原语 → 免弱/附加禁令自然接管）。
//   - 当次攻击也算"窗内"：470 在注册窗口前先跑一次 tick；本宏载体 28016 是属性技
//     （executed_skill_is_attack=false）→ 当次 no-op，后续攻击刀才触发。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   24955 [585,57,965] / 25224 [48,850,965] / 25693 [48,47,965] / 27116 [1085,965,843] /
//   **28016 祷灵祭坛 [965]|[3,1]（纯链，场景载体）** / 28222 [911,965,843] /
//   28523 [585,965,481] / 37499 [950,965,833]。
//
// 动工前置三查（2026-10-22）：①内核 grep 965 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5740_hit_all_down_window.cpp（号段 5740-5749）。

#include "common.h"

namespace {

// ── 965 窗口 tick —— args=[owner, opp, levels]（register_round_effect 载荷）──
EffectResult j243_965_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int levels = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp || levels <= 0) {
        return EffectResult::kOk;
    }
    // 只对"自身攻击技能命中"响应：属性技不算；miss/被盔在桶入口已被 SKILL_INVALID 过滤，
    // 这里再按判据统一口径自卡一次（HIT_INVALID 仍是 HIT，效果照常触发）。
    if (!executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, levels, owner);
    }
    return EffectResult::kOk;
}

// ── 965 —— {0}回合内自身攻击技能命中则对手全属性-{1} ──
EffectResult effect_965_hit_all_down_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 965, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 965, args,
                     "965 布局=[owner,target,rounds,levels]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int levels = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 965,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：0/负档 = 不注册窗口（不回退默认值）。
    if (rounds <= 0 || levels <= 0) {
        return EffectResult::kOk;
    }
    // 当次攻击也属窗内（470 同款：先跑一次 tick；属性技载体在此自然 no-op）。
    j243_965_tick(ctx, EffectArgs(std::vector<int>{owner, opp, levels}));
    // 后续回合：双 ON_SKILL_HIT 桶（结构性命中门控）。
    for (const State st :
         {State::BATTLE_FIRST_ON_SKILL_HIT, State::BATTLE_SECOND_ON_SKILL_HIT}) {
        register_round_effect(ctx, st, owner, /*effect_id=*/965, rounds,
                              {owner, opp, levels}, &j243_965_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(965, effect_965_hit_all_down_window)
