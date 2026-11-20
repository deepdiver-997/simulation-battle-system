// J 组 J281 · 疲惫概率递增族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   465 [4] "{0}%令对手疲惫{1}回合，每次使用概率提升{2}%，最高{3}%"
//
// 实现口径（1328 递增范式直系，j_count_output.cpp）：
//   - uses 计数存 plugin_storage（owner×技能分账；SKILL_EFFECT 每用 +1、跨回合不重置、
//     换技不断账——782/1328 已定口径）。首次使用 uses=1 → 申报概率 {0}；
//     第 n 次 = min({0}+(n-1)×{2}, {3})（**命中也不清零**：模板是"每次使用概率提升"，
//     不是"未触发则提升"）。
//   - 施加 = `apply_anomaly_checked(对手, 疲惫码=7, duration={1}, actor=owner,
//     chance=申报概率, ChanceSource::Skill)`——现代施加链（次免→弹控→抗性→魂免→
//     转化→落地）+ 概率闸门；掷骰只在原语内发生一次（插件不再另掷）。
//   - 审计槽：991465000+owner = 本次申报概率；991465100+owner = 本次施加结果
//     （ApplyAnomalyResult 枚举值）；991465200+owner = 本次 uses。
//   - pct 类行参在位即消费：{0}=0 仍登记审计并按原语掷（0 = 永不触发）；
//     {2}=0 = 概率不递增；{3} 为上限（载体可 <100）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   465 [chance, duration, step, cap]：13997 盾牌猛击 [40,2,10,100]（纯链，场景载体）、
//     14609 钢铁螺旋击 [20,2,5,40]、15339 烈龙青影斩 [50,1,25,100]（纯链，duration=1
//     场景载体）、15654 机械天雷盾 [30,2,20,70]、15884 妖异扭曲术 [20,2,10,60]、
//     17995 超能圣盾破 [20,2,10,50]、32440 美食大狂欢 [40,2,10,60]、
//     36816 苍穹幻澜 [60,2,20,100]。
//
// 动工前置三查（2026-10-22）：①内核 grep 465 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(465) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6140_fatigue_ramp.cpp（号段 6140-6149）。

#include "common.h"

namespace {

constexpr int kFatigue = 7;   // 官方异常码 7 = 疲惫

// ── plugin_storage 键段（994650x，grep 全库无撞）──
constexpr int kUsesKeyBase = 994650100;       // +owner*40000 + skill_id%40000：已用次数
constexpr int kChanceAuditBase = 991465000;   // +owner：本次申报概率
constexpr int kApplyAuditBase = 991465100;    // +owner：本次施加结果
constexpr int kUsesAuditBase = 991465200;     // +owner：本次 uses

// ── 465 —— {0}%令对手疲惫{1}回合，每次使用概率+{2}%，最高{3}% ──
// int_args = [owner, target, base, duration, step, cap]（args_num=4）。
EffectResult effect_465_fatigue_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 465, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 465, args,
                     "465 布局=[owner,target,基础概率,疲惫回合,递增,上限]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base = args.int_args[2];
    const int duration = args.int_args[3];
    const int step = args.int_args[4];
    const int cap = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     465, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base >= 0 && base <= 100, 465, args, "基础概率须在 [0,100]");
    EFFECT_ARG_GUARD(duration > 0, 465, args, "疲惫回合须 > 0");
    EFFECT_ARG_GUARD(step >= 0, 465, args, "递增须 >= 0");
    EFFECT_ARG_GUARD(cap >= 0 && cap <= 100, 465, args, "上限须在 [0,100]");

    // uses 计数（owner×技能 分账；取不到技能槽退化为 skill_id=0 独立账本，1328 同款）。
    const int slot = ctx->executing_skill_slot(owner);
    const int skill_id = (slot >= 0 && slot < 5)
                             ? ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id
                             : 0;
    const int key = kUsesKeyBase + owner * 40000 + (skill_id % 40000);
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    ctx->plugin_storage[kUsesAuditBase + owner] = uses;

    const int chance = std::min(base + (uses - 1) * step, cap);
    ctx->plugin_storage[kChanceAuditBase + owner] = chance;
    ctx->plugin_storage[kApplyAuditBase + owner] = -1;
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult res =
        g_core->apply_anomaly_checked(ctx, opp, kFatigue, duration, owner, chance,
                                      ChanceSource::Skill);
    ctx->plugin_storage[kApplyAuditBase + owner] = static_cast<int>(res);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(465, effect_465_fatigue_ramp)
