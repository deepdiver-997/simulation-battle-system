// J82 · 受击转化/先手免反族 —— 1314×16 / 138×16（2026-10-03，worktree ../agents/j82）
//
// 家族语义（effect_info 官方模板 + 载体切片 + 4399 文本核对）：
//
// 1314（args_num=1）"令自身下{0}次受到的攻击伤害转化为体力"：
//   - EVENT_TAKE_DAMAGE watcher（J24 153 事件面直系：ev.target==self、NORMAL 红伤、
//     amount>0）+ shared_ptr 计次（{0} 次用完闭包闲置）；
//   - "转化为体力" = 受击后按 ev.amount 全额 heal_amount（净伤 0——受击台账口径，
//     工单既定通道；非 RuleCenter 免票，不拦伤害只等额回）。
//   - 实例：幽夜漂浮 28588 [1314]|[1]（属性技纯链）。
// 138（args_num=2）"先出手时，{0}回合自己不会受到对手攻击性技能伤害并反弹对手
//   造成伤害的1/{1}"：
//   - 先手门 = SKILL_EFFECT 时点 acting==self 且 self 为先手方（J69 483 同判据正向；
//     先手权在 SKILL_EFFECT 前已定）——门不过不武装；
//   - 免伤窗 = BLOCK 相位闭包（816 arm_immunity_reflect_true 同机制：读被归零前
//     final→整段归零），计次改为**回合窗**：{0} 回合（含当回合——先手免伤的意义就是
//     挡当回合后手那一下；闭包捕获 end_round，roundCount 越界即闲置）；
//   - 反弹 = 免疫量 × 1/{1} 走 deal_pink_damage(FIXED)（犀牛 1217/K_thorn 反弹通道；
//     816 原版是真伤，本条模板无"真实"字样 → 取可被粉抗管辖的 FIXED 档，记档）。
//   - 实例：光之盾 21328 [138]|[1,2]（属性技纯链：1 回合、反弹 1/2）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1314/138 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1314=1/138=2 与 effect_info.args_num 一致；32 载体切片自洽。
// 先例：J24 153（TAKE_DAMAGE watcher 事件面）、damage_modifier.cpp 816
//   （arm_immunity_reflect_true BLOCK 闭包）、K_thorn（反弹 FIXED 通道）、J69（先手门）。
// 场景回归：test/scenario/scenario_1720_taken_convert_reflect.cpp（1720 号段）。

#include "common.h"

#include <memory>
#include <utility>

// ── 1314 —— 令自身下{0}次受到的攻击伤害转化为体力 ──
// int_args = [owner, target, counts]（args_num=1）。
EffectResult effect_1314_taken_convert_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1314, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1314, args,
                     "1314 布局=[owner,target,counts]");
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1314, args, "owner 越界");
    EFFECT_ARG_GUARD(counts > 0, 1314, args, "需要 counts>0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    auto remaining = std::make_shared<int>(counts);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, remaining](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || *remaining <= 0 || !g_core
                || !g_core->heal_amount) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到的攻击伤害" = 红伤
            }
            --*remaining;
            (void)g_core->heal_amount(c, self, ev.amount);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 138 —— 先出手时，{0}回合自己不会受到对手攻击性技能伤害并反弹 1/{1} ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_138_first_parry_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 138, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 138, args,
                     "138 布局=[owner,target,rounds,denom]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 138, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 138, args, "需要 回合/分母 > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 先手门：SKILL_EFFECT 时点先手权已定（owner 即出招方，无需 acting 过滤）；
    // 后手不武装。
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (self != first) {
        return EffectResult::kOk;
    }
    // BLOCK 相位闭包（816 同机制改回合窗）：{0} 回合含当回合。
    const int end_round_exclusive = ctx->roundCount + rounds;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, denom, end_round_exclusive](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (c->roundCount >= end_round_exclusive) {
                return;   // 窗过期闲置
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只免"对手的攻击技能伤害"（红伤）
            }
            const int attacker = d.attackerId;
            const int immune_amount = d.final;   // 被归零前的伤害值
            if (attacker < 0 || attacker > 1) {
                return;
            }
            // 归零（整段快照，816 同款）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // 反弹免疫量 1/{denom}（FIXED 粉伤通道，1217 反弹口径）
            const int reflect = immune_amount / denom;
            if (reflect > 0) {
                (void)g_core->deal_pink_damage(c, attacker, reflect, DamageKind::FIXED, self);
            }
        });
    return EffectResult::kOk;
}

SKILL_EFFECT(1314, effect_1314_taken_convert_heal)
SKILL_EFFECT(138, effect_138_first_parry_reflect)
