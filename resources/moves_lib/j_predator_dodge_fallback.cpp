// J 板 J260 · 天敌控制兜底闪避窗族 —— 1489×10（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 1489，args_num=4）：
//   "若对手为自身天敌则{0}%令对手{1}，未触发则令自身{2}回合内{3}%闪避对手攻击"
//   {0}=申报概率 {1}=异常码 {2}=闪避窗回合数 {3}=闪避百分比。
//
// 载体切片（side_effect.arg_count 顺序切分，10/10 自洽）：
//   28235 雾弥抚骨 [100,28,2,70]      28889 骇浪飘摇 [70,15,3,70]
//   28269 辟魔之辉 [191,813,1489]|[4,80,7,3,80] → 1489=[80,7,3,80]
//   34742 苍魂融天爪 [913,1489,582]|[100,100,2,2,100,2] → 1489=[100,2,2,100]
//   36416 极龙灭爪 [1489,1451]|[50,2,2,50,250,3] → 1489=[50,2,2,50]
//   36569 悼 [1489,842]|[50,7,1,100,75] → 1489=[50,7,1,100]
//   36602 纵魔之法 [70,6,2,100]       36746 悼玄黄 [868,1489,842]|[3,100,7,2,100,100]
//        → 1489=[100,7,2,100]         36902 高空箭翎 [986,1489]|[70,70,10,2,70]
//        → 1489=[70,10,2,70]           37028 雷封百步 [70,0,3,70]
//
// 实现口径：
//   - 天敌门 = `is_predator(ctx, owner)`（common.h：对手固有属性克制自身固有属性，
//     1488/1491/2057 同款）；门闭整条不动作。
//   - 主干 = apply_anomaly_checked **申报制**（916/J3 口径）：概率 {0} 走概率闸门，
//     异常 {1} 走现代施加（免疫/抗性/弹控/转化全链生效）；"未触发" = !landed
//     （含 ROLL_FAILED 与各被挡档，1482/1488 同判据）。
//   - 兜底 = {2} 回合闪避窗：当回合直写 `ws.hit_rate_mod[对手] *= (1 - {3}/100)`
//     + register_round_effect(ROUND_START, rounds={2}) 每回合重写（ws 每回合 reset，
//     1390 直系 j_prob_anomaly_fallback.cpp；后出手方起点顺延由
//     round_effect_start_round 折算）。100% 档 = 必 miss（必中技仍可穿，K7 内核口径）。
//   - 红线 7：{3}=0 在位即消费——窗照常注册、乘区写 ×1.0（等价空操作），不用 >0 守卫
//     把显式 0 档吞掉；{2}=0 无窗可开（无"在位"语义）。
//   - 审计槽（场景无随机断言用）：996489100+owner = 天敌门（1/0）；
//     996489200+owner = 主句落地（1/0）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1489 无行为硬编码 ②effect_info 模板唯一
//   args_num=4、custom_effect_programs/overrides 零行 ③全 worktree moves_lib/soul_lib
//   无 SKILL_EFFECT(1489) 注册——真缺口。注：运行时文本 parser 能误匹配主句
//   "令对手{"（会忽略天敌门），但 `clone_effect` 只在 `!effect.logic` 时才走 parser，
//   本注册后插件优先，parser 不再接手。
// 场景回归：test/scenario/scenario_5910_predator_dodge_fallback.cpp（号段 5910-5919）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k1489PredatorAuditBase = 996489100;   // +owner：1=天敌门过 / 0=门闭
constexpr int k1489LandedAuditBase = 996489200;     // +owner：1=主句落地 / 0=未落地

// 现代施加的"落地"判定（j_prob_fallback3/J3 同口径）。
bool j260_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1489 兜底跳：ROUND_START 重写对手命中率乘区（ws 每回合 reset → 逐回合重写）──
EffectResult j260_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (opp < 0 || opp > 1 || pct < 0) {
        return EffectResult::kOk;
    }
    // 红线7：pct=0 在位即消费（×1.0 等价空操作，仍逐回合走一遍窗）。
    ctx->ws.hit_rate_mod[opp] *= (1.0f - static_cast<float>(pct) / 100.0f);
    return EffectResult::kOk;
}

// ── 1489 —— 若对手为自身天敌则{0}%令对手{1}，未触发则令自身{2}回合内{3}%闪避 ──
// int_args = [owner, target, 概率, 异常, 回合, 闪避%]（args_num=4）。
EffectResult effect_1489_predator_anomaly_or_dodge(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1489, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1489, args,
                     "1489 布局=[owner,target,概率,异常,回合,闪避%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int dodge = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1489, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1489, args,
                     "{1} 须为合法异常码");
    EFFECT_ARG_GUARD(chance >= 0 && rounds >= 0 && dodge >= 0, 1489, args,
                     "需要 概率/回合/闪避% >= 0（在位即消费）");

    const bool predator = is_predator(ctx, owner);
    ctx->plugin_storage[k1489PredatorAuditBase + owner] = predator ? 1 : 0;
    if (!predator) {
        return EffectResult::kOk;   // 天敌门闭：主句与兜底都不动作
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    const bool landed = j260_landed(r);
    ctx->plugin_storage[k1489LandedAuditBase + owner] = landed ? 1 : 0;
    if (landed) {
        return EffectResult::kOk;
    }

    // 未触发 → 闪避窗：当回合直写 + ROUND_START 逐回合重写（1390 双写口径）。
    if (rounds > 0) {
        ctx->ws.hit_rate_mod[target] *= (1.0f - static_cast<float>(dodge) / 100.0f);
        register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1489, rounds,
                              {owner, target, dodge}, &j260_dodge_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1489, effect_1489_predator_anomaly_or_dodge)
