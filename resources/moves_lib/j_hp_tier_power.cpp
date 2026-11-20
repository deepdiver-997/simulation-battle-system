// J102 · 血量阈值变威力族 —— 1252×15（2026-10-03，worktree ../agents/j102）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1252（args_num=4）"自身体力低于1/{0}时造成的伤害为{1}倍，低于1/{2}时伤害为{3}倍"：
//   - 变威力视图：SKILL_EFFECT 时点 ws.skill_power_view 已被引擎物化为基线威力
//     （ON_SKILL_HIT materialize，40/30 同款）→ 按自身当前 hp 分层乘写回
//     （operator= 置 rewritten → 走标准变威力重算管线）；
//   - 分层顺序 = 深档优先：hp < max/{2} → ×{3}；否则 hp < max/{0} → ×{1}；
//     两档都不满足（血量健康）→ 不动。max 用 max_hp_of（common.h）。
//   - 实例：祭魂锋刃 31254 [889,1252]|[6,2,2,3,3]（物85 prio3 纯链；889=消对手回合类
//     已注册 selection_seal——空场消除失败=良性）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1252 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1252=4 与 effect_info.args_num 一致；15 载体切片自洽（[分母,倍率,分母,倍率]）。
// 先例：40/30（damage_modifier.cpp 威力视图写回）、882（每提升威力）、max_hp_of。
// 场景回归：test/scenario/scenario_1950_hp_tier_power.cpp（1950 号段）。

#include "common.h"

namespace {

}  // namespace

// ── 1252 —— 自身体力低于1/{0}时伤害{1}倍，低于1/{2}时{3}倍（深档优先）──
// int_args = [owner, target, denom1, mult1, denom2, mult3]（args_num=4）。
EffectResult effect_1252_hp_tier_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1252, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1252, args,
                     "1252 布局=[owner,target,denom1,mult1,denom2,mult3]");
    const int self = args.int_args[0];
    const int denom1 = args.int_args[2];
    const int mult1 = args.int_args[3];
    const int denom2 = args.int_args[4];
    const int mult3 = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1252, args, "owner 越界");
    EFFECT_ARG_GUARD(denom1 > 0 && denom2 > 0 && mult1 > 0 && mult3 > 0, 1252, args,
                     "需要 分母/倍率 > 0");
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const int hp = ctx->getPet(self).hp;
    int mult = 0;
    if (hp < max_hp / denom2) {
        mult = mult3;   // 深档优先
    } else if (hp < max_hp / denom1) {
        mult = mult1;
    }
    if (mult > 0) {
        view = view.value * mult;
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1252, effect_1252_hp_tier_power)
