// J 组 J278 · 概率倍伤递增族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1242 [4] "{0}%的概率伤害为{1}倍，未触发则下次翻倍概率额外提升{2}%且伤害提升变为{3}倍"
//
// 实现口径（1328 递增范式直系，j_count_output.cpp）：
//   - SKILL_EFFECT 时点掷骰（插件本地 roll_percent，482 同层口径；伤害类概率无"落地"
//     可闸，不走 apply_anomaly_checked）。
//   - 命中 → `ws.damage_add_pct[owner] += ({本次倍率}-1)×100`（damage_add_pct 是加算
//     百分点通道，×2 → +100；SKILL_EFFECT 在伤害管线 AMP 之前，1328/batch_b 同路）。
//   - 未触发 → 账本（plugin_storage，owner 分账）记录连续未触发次数 k：
//       下次申报概率 = min({0} + k×{2}, 100)，下次倍率 = {3}；
//       首次使用 k=0 → 概率 {0}、倍率 {1}。命中后 k 清零（回到首次档）。
//   - 审计槽（场景无随机断言用）：
//       991242000+owner = 本次申报概率；991242100+owner = 本次是否命中（1/0）；
//       991242200+owner = 本次实际倍率；991242300+owner = 当前连续未触发次数。
//   - pct 类行参在位即消费：{0} 显式 0 = 永不触发（但仍登记审计槽）；{2}=0 = 概率不递增。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 10/10 自洽）：
//   1242 [prob, mult, step, esc_mult]：31116 有女初长成 [20,5,15,6]、31757 噬魂幽芒
//     [20,2,10,3]、32264 净世繁花诀 [20,2,10,3]、32308 天·混沌之瞳 [50,2,25,3]（场景载体）、
//     32651 燃魂漩涡 [25,2,10,3]、33462 魔钢碎魂击 [20,2,10,3]、34071 凛风骤攻 [30,2,20,3]、
//     34605 皓腕凝霜舞 [20,2,10,3]、37976 呼风破浪 [50,2,20,3]、38310 噬心劫 [30,2,20,3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1242 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(1242) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6110_prob_mult_ramp.cpp（号段 6110-6119）。

#include "common.h"

namespace {

// ── plugin_storage 键段（991242x，grep 全库无撞）──
constexpr int kChanceAuditBase = 991242000;   // +owner：本次申报概率
constexpr int kProcAuditBase = 991242100;     // +owner：本次是否命中（1/0）
constexpr int kMultAuditBase = 991242200;     // +owner：本次实际倍率
constexpr int kMissStreakBase = 991242300;    // +owner：连续未触发次数

// ── 1242 —— {0}%概率伤害为{1}倍；未触发则下次概率+{2}%且倍率变{3}倍 ──
// int_args = [owner, target, prob, mult, step, esc_mult]（args_num=4）。
EffectResult effect_1242_prob_mult_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1242, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1242, args,
                     "1242 布局=[owner,target,概率,倍率,递增,未触发倍率]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base = args.int_args[2];
    const int mult = args.int_args[3];
    const int step = args.int_args[4];
    const int esc_mult = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1242, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base >= 0 && base <= 100, 1242, args, "概率须在 [0,100]");
    EFFECT_ARG_GUARD(mult >= 1 && esc_mult >= 1, 1242, args, "倍率须 >= 1");
    EFFECT_ARG_GUARD(step >= 0, 1242, args, "递增须 >= 0");

    const int key = kMissStreakBase + owner;
    const auto it = ctx->plugin_storage.find(key);
    const int streak = std::min(it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second),
                                100);
    // 首次 = {0}/{1}；连续未触发 k 次后 = min({0}+k×{2},100)/{3}。
    const int chance = std::min(base + streak * step, 100);
    const int used_mult = (streak > 0) ? esc_mult : mult;
    ctx->plugin_storage[kChanceAuditBase + owner] = chance;
    ctx->plugin_storage[kMultAuditBase + owner] = used_mult;

    const bool proc = roll_percent(chance);
    ctx->plugin_storage[kProcAuditBase + owner] = proc ? 1 : 0;
    if (proc) {
        // {N}倍 = +(N-1)×100 加算百分点（1328 同款）。
        ctx->ws.damage_add_pct[owner] += static_cast<float>((used_mult - 1) * 100);
        ctx->plugin_storage[key] = 0;
    } else {
        ctx->plugin_storage[key] = streak + 1;
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1242, effect_1242_prob_mult_ramp)
