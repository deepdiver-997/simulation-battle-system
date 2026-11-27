// J 组 J246 · 护盾概率翻倍族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1486（args_num=1）"{0}%概率造成伤害翻倍，自身处于护盾状态时概率翻倍"
//
// 实现口径：
//   - 有效概率：自身 `shield_bank_.total() > 0` → min(100, {0}×2)，否则 {0}。
//     `{0}` 在位即消费：<=0 显式关闸（不回退默认值）。
//   - 掷点 = roll_percent(有效概率)（0/100 短路不消耗 rand）；命中 →
//     `ws.damage_add_pct[owner] += 100`（"伤害翻倍"= 通用增伤 +100%，890/858 同通道；
//     SKILL_EFFECT 写入，AMP 阶段读取）。
//   - 审计槽：991486100+owner = 本次有效概率（百分点）；991486200+owner = 本次掷点结果
//     （1=翻倍/0=未翻）。场景据此做无随机精确断言 + 统计带侧证。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   33558 [942,1486]|[3,50] / **35201 灰烬傀儡 [1486]|[50]（纯链，场景载体）** /
//   35335 [1582,1486]|[50,7,50,6,300,25] / 35735 [901,1486,984]|[2,0,0,0,2,0,50,1] /
//   36567 [828,1486,795]|[50,20,100] / 36941 [1487,1486,894]|[50,250] /
//   **37170 无觉之目 [1486]|[50]（纯链）** / **37260 风哗冰斩 [1486]|[50]（纯链）**。
//   全库实载 {0} 只有 50。
//
// 动工前置三查（2026-10-22）：①内核 grep 1486 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5770_shield_chance_double.cpp（号段 5770-5779）。

#include "common.h"

namespace {

constexpr int k1486ChanceBase = 991486100;   // +owner：本次有效概率（百分点）
constexpr int k1486OutcomeBase = 991486200;  // +owner：本次掷点结果 1/0（审计）

void set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1486 —— {0}%概率造成伤害翻倍，自身处于护盾状态时概率翻倍 ──
EffectResult effect_1486_shield_chance_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1486, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1486, args,
                     "1486 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1486,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不翻倍（不回退默认值）。
    if (pct <= 0) {
        set_storage_int(ctx, k1486ChanceBase + owner, 0);
        set_storage_int(ctx, k1486OutcomeBase + owner, 0);
        return EffectResult::kOk;
    }
    int chance = pct;
    if (ctx->getPet(owner).shield_bank_.total() > 0) {
        chance = std::min(100, pct * 2);   // "处于护盾状态时概率翻倍"（封 100）
    }
    set_storage_int(ctx, k1486ChanceBase + owner, chance);
    const bool doubled = roll_percent(ctx->rng(), chance);
    set_storage_int(ctx, k1486OutcomeBase + owner, doubled ? 1 : 0);
    if (doubled) {
        ctx->ws.damage_add_pct[owner] += 100.0f;   // "伤害翻倍"= 通用增伤 +100%
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1486, effect_1486_shield_chance_double)
