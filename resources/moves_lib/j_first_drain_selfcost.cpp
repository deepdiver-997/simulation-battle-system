// J 组 J72 · 先手吸血/自耗反噬族 —— 458×15 / 1185×16（2026-10-03，worktree ../agents/j72）
//
// 家族语义（effect_info 官方模板即权威）：
//   458（arg_count=1）"若先出手则造成攻击伤害的{0}%恢复自身体力"
//   1185（arg_count=1）"消耗自身当前最大体力的1/{0}并使对手受到等量百分比伤害"
//
// 实现口径：
//   - 458 = 内核 effect_register_state case 458 归位 BATTLE_FIRST_AFTER_ACTION
//     （101/422 组内插一行，2026-10-03）→ AFTER_ACTION 时点：先手门
//     （acting_side_is_first，J57 1092 同判据正向）+ 裸伤台账
//     ws.raw_attack_damage[owner]（2440 bailuo 同款读法——挡伤/锁伤改不了它）
//     × {0}%/100 → heal_amount（417/101 同通道）。
//   - 1185 = SKILL_EFFECT 直通（沧岚拆分先例）：cost = 自身**当前**体力/{0}
//     （"当前最大体力"以当前体力计，J72 工单裁定）→ deal_pink_damage(self,
//     cost, FIXED) 自耗 + deal_pink_damage(target, cost, PERCENT_VALUE) 反噬
//     （"等量百分比伤害"= 点数等量、粉伤通道吃护罩）。SKILL_EFFECT 时点早于
//     本攻击的伤害阶段——自耗不叠加进本次红伤。
//
// 载体切片（unity 库 json_each 精确匹配 15/16=31；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   458 [pct]：{50}×7/{30}×4/{100}×3/{25}×1（纯链载体 极月无双破 13871 [50] 特150）
//   1185 [分母]：{2}×14/{1}×1/{3}×1（纯链载体 熄灭光明 31170 [2] 物130）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 458/1185 无行为硬编码
// （effect_register_state 无行=本次补的行）②effect_meta 无行 ③moves_lib 无注册
// ——真缺口。
// 先例：101/422（AFTER_ACTION 裸伤台账组）、2440（raw_attack_damage 读法+heal，
// bailuo）、 acting_side_is_first（common.h，J57）、沧岚 581（deal_pink 分段拆分）。
// 场景回归：test/scenario/scenario_1590_first_drain_selfcost.cpp（1590 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 458 —— 若先出手则造成攻击伤害的{0}%恢复自身体力（AFTER_ACTION）──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_458_first_strike_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 458, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 458, args,
                     "458 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 458, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 458, args, "需要 pct > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门：后手无事发生
    }
    const int dealt = ctx->ws.raw_attack_damage[owner];
    if (dealt <= 0 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;   // 未造成攻击伤害（被挡/ miss）不回
    }
    (void)g_core->heal_amount(ctx, owner, dealt * pct / 100);
    return EffectResult::kOk;
}

// ── 1185 —— 消耗自身当前体力的1/{0}并使对手受到等量百分比伤害 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1185_selfcost_recoil(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1185, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1185, args,
                     "1185 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1185, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1185, args, "需要 分母 > 0");
    const int cost = ctx->getPet(owner).hp / denom;
    if (cost <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 自耗（FIXED 粉：吃护罩不减体力则 dealt=0——不回收）+ 等量反噬（PERCENT_VALUE 粉）。
    g_core->deal_pink_damage(ctx, owner, cost, DamageKind::FIXED, owner);
    g_core->deal_pink_damage(ctx, opp, cost, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(458, effect_458_first_strike_heal)
SKILL_EFFECT(1185, effect_1185_selfcost_recoil)
