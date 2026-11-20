// J 组 J28 · 解弱闪避/先手免伤族 —— 3 宏/63 载体（2026-10-02，worktree ../agents/j28）
//
// 家族语义（effect_info 模板即权威）：
//   568  [1] 解除自身能力下降状态，若解除成功则{0}回合内躲避所有攻击
//   664  [0] 若先出手则当回合对手无法造成攻击伤害
//   1018 [2] {0}回合内若受到的伤害低于{1}则免疫伤害并造成等同于伤害量的固定伤害
//
// 实现口径：
//   - 568 = clear_stat_drops 门（CoreApi 原语，返回清掉个数 >0 = "解除成功"；835 的
//     stat_reversal 是"反转"、本模板是"解除"——语义不同原语不同）+ 1069 全躲避窗
//     （ws.dodge_rate=1.0 当回合直写 + ROUND_START InRounds tick 重写，426 双写形；
//     j10_dodge_all_tick 同款——必中技可穿 = 内核统一行为，记档）。
//   - 664 = acting_side_is_first 门（980/52 判据）→ 850 式 DAMAGE 免票
//     （rule_center_.grant_immune，duration=1 恰当回合，counts=0）。
//   - 1018 = J3 1278/1727 的 816 BLOCK 闭包变体（阈值版）：BLOCK 相位读**归零前**
//     final（< {1} 判据）→ 全字段归零（免疫）+ deal_pink_damage(FIXED) 反等量；
//     InRounds {0} 窗（lambda 自检 roundCount，1236 形）。每次受击独立判定
//     （模板无次数限定）。
//
// 载体切片（unity 库 json_each 精确匹配 21×3=63；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   568  [rounds]：28138 鲨斗之盾 [2]、23005 万念俱灰 [3]
//   664  全部 0 参：16477 瞬光天闪、25655 意识初醒…
//   1018 [rounds,阈值]：26473 美味佳肴 [3,300]、29037 叹息水墙 [3,1000]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/
// overrides 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1130_dispel_dodge_gates.cpp（号段 1130-1139）。

#include "common.h"

#include <memory>

namespace {

// 568 的 ROUND_START tick —— ws 每回合 memset → 窗口每回合重写（1069 同款）。
EffectResult j28_568_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = 1.0f;   // "躲避所有攻击"（必中技可穿=内核统一行为）
    return EffectResult::kOk;
}

// ── 568 —— 解除自身能力下降状态，若解除成功则{0}回合内躲避所有攻击 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_568_dispel_dodge_all(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 568, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 568, args,
                     "568 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     568, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 568, args, "需要 回合 > 0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    // "解除成功" = 清掉的下降档个数 >0（无下降 → 0 → 不触发）。
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    // 426 形：当回合立即生效 + ROUND_START 窗重写（ws 每回合 memset）。
    ctx->ws.dodge_rate[self] = 1.0f;
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 568, rounds, {self},
                          &j28_568_dodge_tick);
    return EffectResult::kOk;
}

// ── 664 —— 若先出手则当回合对手无法造成攻击伤害 ──
// int_args = [owner, target]（args_num=0，装载器注入）。
EffectResult effect_664_first_attack_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 664, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 664, args,
                     "664 布局=[owner,target]（0 参效果）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     664, args, "需要 owner/target ∈ {0,1}");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门（980 同判据：后手无事发生）
    }
    // 当回合免攻击伤害票（850 同款：duration=1 恰当回合、counts=0、ON_STAGE）。
    if (ctx) {
        ctx->rule_center_.grant_immune(
            self, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
            /*duration_rounds=*/1, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
            /*source_effect_id=*/664);
    }
    return EffectResult::kOk;
}

// ── 1018 —— {0}回合内若受到的伤害低于{1}则免疫伤害并造成等同于伤害量的固定伤害 ──
// int_args = [owner, target, rounds, threshold]（args_num=2）。恒久窗内逐次判定。
EffectResult effect_1018_low_hit_immune_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1018, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1018, args,
                     "1018 布局=[owner,target,回合,阈值]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1018, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 1018, args, "需要 回合/阈值 > 0");
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, threshold, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外（InRounds，1236 形自检）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只对对手攻击伤害（红伤）判定
            }
            const int attacker = d.attackerId;
            if (attacker < 0 || attacker > 1 || d.final >= threshold) {
                return;   // "受到的伤害低于{1}"（严格小于）——不及阈值照常承伤
            }
            const int immune_amount = d.final;   // 反弹量 = 被归零前伤害值（816 闭包口径）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            if (g_core && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(c, attacker, immune_amount, DamageKind::FIXED,
                                               self);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(568, effect_568_dispel_dodge_all)
SKILL_EFFECT(664, effect_664_first_attack_immune)
SKILL_EFFECT(1018, effect_1018_low_hit_immune_reflect)
