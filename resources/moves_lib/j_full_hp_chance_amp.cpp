// J 组 J249 · 满体概率增伤族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1525（args_num=2）"{0}%概率造成的攻击伤害提升{1}%，若出手时自身为满体力则概率翻倍"
//
// 实现口径：
//   - 有效概率：出手时 `hp == max_hp` → min(100, {0}×2)，否则 {0}。
//     `{0}`/`{1}` 在位即消费：<=0 显式关闸（不回退默认值）。
//   - 掷点 = roll_percent(有效概率)；命中 → `ws.damage_add_pct[owner] += {1}`
//     （通用增伤加算通道；SKILL_EFFECT 写入，AMP 阶段读取）。
//   - 审计槽：991525100+owner = 本次有效概率（百分点）；991525200+owner = 掷点结果（1/0）。
//   - "出手时满体力"读**当前** hp（FSM 结算到 SKILL_EFFECT 时的现场值；本宏在本次
//     技能链内、伤害阶段之前，后续伤害不影响本次判定）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   **33836 冥牙嗜 [950,1525]|[2,50,100]（物 85；950 清对手回合效果，无则 no-op）** /
//   34355 [867,1525]|[2,50,100] / 34958 [1032,1549,1525]|[1,1,50,100] /
//   35209 [1170,1525]|[300,50,100] / 35463 [1523,1525,669]|[300,50,100] /
//   35571 [1525,1084]|[50,100,200] / 35666 [995,1525]|[3,50,100] /
//   35725 [1525,1258]|[50,100,350,1]。全库实载 {0}=50 / {1}=100。
//
// 动工前置三查（2026-10-22）：①内核 grep 1525 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5800_full_hp_chance_amp.cpp（号段 5800-5809）。

#include "common.h"

namespace {

constexpr int k1525ChanceBase = 991525100;   // +owner：本次有效概率（百分点）
constexpr int k1525OutcomeBase = 991525200;  // +owner：本次掷点结果 1/0（审计）

void set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1525 —— {0}%概率伤害提升{1}%，出手时满体力则概率翻倍 ──
EffectResult effect_1525_full_hp_chance_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1525, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1525, args,
                     "1525 布局=[owner,target,chance,boost]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance_arg = args.int_args[2];
    const int boost = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1525,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不增伤（不回退默认值）。
    if (chance_arg <= 0 || boost <= 0) {
        set_storage_int(ctx, k1525ChanceBase + owner, 0);
        set_storage_int(ctx, k1525OutcomeBase + owner, 0);
        return EffectResult::kOk;
    }
    int chance = chance_arg;
    if (ctx->getPet(owner).hp == max_hp_of(ctx, owner)) {
        chance = std::min(100, chance_arg * 2);   // "出手时自身为满体力则概率翻倍"
    }
    set_storage_int(ctx, k1525ChanceBase + owner, chance);
    const bool landed = roll_skill_percent(*ctx, 1525, 0, owner, opp, chance);
    set_storage_int(ctx, k1525OutcomeBase + owner, landed ? 1 : 0);
    if (landed) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(boost);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1525, effect_1525_full_hp_chance_amp)
