// J16 · 吸取条件族 —— 1257×24 / 1109×22 / 1290×26（2026-10-02，worktree ../agents/j16）
//
// 家族语义（effect_info 官方模板 + 4399 无相谛全文核对），三条全是"条件 → 吸取/恢复"：
//
// 1257（arg_count=1）"对手不处于异常状态则吸取对手最大体力的1/{0}"：
//   - 条件 = !has_any_active_abnormal_status(对手)（任一异常在身即否——4399 无相谛
//     "对手处于异常状态时100%令对手臣服"是链上**另一条效果**，不归本条管）；
//   - 吸取 = g_core->drain_hp（CoreApi 明注"无相谛 1257 同族"：打 1/{0} 对手最大体力、
//     按实际落地回吸）——**本条原走引擎 effect_unit 文本解析**（effect_unit_parser
//     第二刀五模板之一：TargetNoAnomaly + DrainHp，对未注册效果的兜底活行为）；
//     插件注册后 loadSkills 的 clone_effect 取到 logic 即**取代**解析路径（不双执行），
//     语义逐字对齐（同条件同原语同 SKILL_EFFECT 时点）。
//   - 实例：无相谛（4399 全文见上）、闪灵击族（全库 24 载体，切片 [denom] 布局零异常）。
// 1109（arg_count=2）"{0}回合内若自身能力提升状态被消除则吸取对手最大体力的1/{1}"：
//   - 窗口 = {0} 回合 InRounds 含当回合（573 同款注册：EVENT_STAT_BOOSTS_LOST
//     watcher、TEAM scope）；触发 = ev.target==self && ev.stat_loss_kind==**0（消除）**
//     ——E13 广播在 clear_stat_boosts（消除，kind=0）/ transfer_stat_boosts（吸取，
//     kind=1）成功路径各发；573 模板带"或吸取"两 kind 都吃，本条模板只有"被消除"
//     → **只吃 kind=0**（对照取严，记档）；
//   - 后果 = drain_hp(self, 对手, {1})，窗口内每次消除独立吸取。
// 1290（arg_count=2）"恢复自身最大体力的1/{0}，若自身体力低于1/{1}则造成等量固伤"：
//   - 恢复 = heal_max_hp_fraction（实际恢复量，吃封回血/恢复修正——common.h 口径）；
//   - 低体判据 = 恢复**前** hp < max/{1}（"恢复体力时若自身体力低于…"4399 同族措辞
//     ——迎击姿态/极风俯冲系；恢复后判会被恢复本身翻案，语义不通）；"等量" =
//     **实际恢复量**（heal=0 即封回血被挡 → 不打 0 伤固伤）；固伤给**对手**（4399
//     迎击姿态"附加**给对手**等量百分比伤害"同族方向）。
//
// 动工前置三查（2026-10-02）：①内核硬编码——1257 命中 effect_unit_parser.cpp 第二刀
//   （文本解析兜底，非 SKILL_EFFECT 级实现；插件化取代之，见上）；1109/1290 内核零命中
//   ②effect_meta 无分类牵连（boss_invalid_effects.json 三 id = "BOSS 战无效"注记）
//   ③moves_lib 无既有注册——真缺口。载体数核对：24/22/26 与工单一致；**全量切片脚本
//   核对 72 载体**：三效果布局零异常（1257 [denom] / 1109 [rounds,denom] /
//   1290 [heal,thresh]）。
// 先例：573（EVENT_STAT_BOOSTS_LOST watcher 窗口，e_state_window.cpp）、drain_hp
//   （CoreApi 163）、heal_max_hp_fraction（common.h）、870/2274（吸取回血写法）。
// 场景回归：test/scenario/scenario_1010_drain_cond.cpp（1010 号段，一文件三宏）。

#include "common.h"

namespace {

// ── 1257 —— 对手不处于异常状态则吸取对手最大体力的1/{0} ──
EffectResult effect_1257_no_anomaly_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1257, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1257, args,
                     "布局=[owner,target,denom]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 1257, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(denom > 0, 1257, args, "需要 denom>0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    if (ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 前提：对手不处于异常状态
    }
    (void)g_core->drain_hp(ctx, self, target, denom);
    return EffectResult::kOk;
}

// ── 1109 —— {0}回合内若自身能力提升状态被消除则吸取对手最大体力的1/{1} ──
EffectResult effect_1109_boost_lost_drain_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1109, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1109, args,
                     "布局=[owner,target,rounds,denom]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1109, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 1109, args, "需要 rounds>0 && denom>0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    // 窗口 watcher（573 同款注册法）：TEAM scope、InRounds {0} 回合含当回合。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, rounds, /*once=*/false,
        [self, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->drain_hp) {
                return;
            }
            // 条件：**自身**的提升被**消除**（kind=0；吸取 kind=1 不算——模板无"或吸取"）。
            if (ev.target != self || ev.stat_loss_kind != 0) {
                return;
            }
            (void)g_core->drain_hp(c, self, 1 - self, denom);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1290 —— 恢复自身最大体力的1/{0}，若自身体力低于1/{1}则造成等量固伤 ──
EffectResult effect_1290_heal_lowhp_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1290, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1290, args,
                     "布局=[owner,target,heal_denom,thresh_denom]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int heal_denom = args.int_args[2];
    const int thresh_denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 1290, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(heal_denom > 0 && thresh_denom > 0, 1290, args,
                     "需要 两分母 > 0");
    if (!g_core || !g_core->heal_amount || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int hp_before = ctx->getPet(self).hp;
    const int healed = heal_max_hp_fraction(ctx, self, heal_denom);
    // 低体判据用恢复前的 hp（恢复后判会被恢复本身翻案）；封回血被挡（healed=0）不打 0 伤。
    if (hp_before >= max_hp_of(ctx, self) / thresh_denom || healed <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(ctx, opp, healed);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1257, effect_1257_no_anomaly_drain)
SKILL_EFFECT(1109, effect_1109_boost_lost_drain_window)
SKILL_EFFECT(1290, effect_1290_heal_lowhp_fixed)
