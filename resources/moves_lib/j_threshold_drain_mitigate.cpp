// J 组 J78 · 阈值吸取/减伤窗族 —— 2 宏/31 载体（2026-10-03，worktree ../agents/j78）
//
// 家族语义（effect_info 模板即权威）：
//   1104 [2] 造成的伤害低于{0}则吸取对手1/{1}最大体力
//   665  [3] 造成的伤害低于{0}则{1}回合内自身受到的伤害减少{2}
//
// 实现口径：
//   - 1104 = 阈值门（内核 case 1104 归位 AFTER_ACTION：final <{0}）→
//     drain_hp(owner, target, {1})（分数形原语 CoreApi 已暴露，1257 通道；
//     掉 max_hp/{1} 固伤 + 自身回等量）。
//   - 665 = 同阈值门（case 665 同组归位）→ InRounds({1}) 红伤扁平减 {2}
//     （REDUCE_FLAT 管线回调 + **武装旗标只注册一次**——J67 实测技能链每回合重跑
//     SKILL_EFFECT 会叠注册；回合界 [r0, r0+{1}-1] lambda 内判，1226 同款）。
//     再触发（窗内再次低伤命中）不刷新窗（记档：首触发定窗）。
//
// plugin_storage 键段（990171x，grep 全库无撞；990178/990179 已被 d5_D19/d5_D15 占用）：
//   990171100 + owner   665 减伤回调已武装旗标
//
// 载体切片（unity 库 json_each 精确匹配 16+15 计账；切片全自洽）：
//   1104 [阈值,分母]：纯链 31492 [300,3]（特150）/36736 [300,3]（物140）/36740 [300,3]（特130）
//   665  [阈值,回合,减量]：无纯链——场景走 34008 [1022,665]|[200,200,2,200]
//        （1022=吸提升成功才吸取，对手无提升即惰性）
//
// 动工前置三查（2026-10-03）：①内核 grep 1104/665 无硬编码 ②custom_effect_overrides
// 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1680_threshold_drain_mitigate.cpp（号段 1680-1689）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k665ArmKeyBase = 990171100;   // +owner：665 减伤回调已武装旗标

int j78_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1104 —— 造成的伤害低于{0}则吸取对手1/{1}最大体力 ──
// int_args = [owner, target, threshold, denom]（args_num=2；内核 case 1104 归位 AFTER_ACTION）。
EffectResult effect_1104_threshold_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || threshold <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 门：伤害低于阈值
    }
    (void)g_core->drain_hp(ctx, owner, target, denom);
    return EffectResult::kOk;
}

// ── 665 —— 造成的伤害低于{0}则{1}回合内自身受到的伤害减少{2} ──
// int_args = [owner, target, threshold, rounds, reduce]（args_num=3；case 665 归位）。
EffectResult effect_665_threshold_mitigate_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int rounds = args.int_args[3];
    const int reduce = args.int_args[4];
    if (owner < 0 || owner > 1 || threshold <= 0 || rounds <= 0 || reduce <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 门：伤害低于阈值
    }
    // 武装旗标：技能链每回合重跑 SKILL_EFFECT，防回调叠注册（J67 实测 R3 减 3×）
    const int arm_key = k665ArmKeyBase + owner;
    if (j78_stor(ctx, arm_key) != 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[arm_key] = 1;
    const int r0 = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [owner, reduce, r0, rounds](BattleContext* c, int) {
            if (!c || c->roundCount < r0 || c->roundCount > r0 + rounds - 1) {
                return;   // 窗口界 [r0, r0+{1}-1]
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed) {
                return;   // 只减打向自身的红伤
            }
            d.final = std::max(0, d.final - reduce);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1104, effect_1104_threshold_drain)
SKILL_EFFECT(665, effect_665_threshold_mitigate_window)
