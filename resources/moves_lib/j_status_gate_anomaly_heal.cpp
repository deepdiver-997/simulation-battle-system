// J 组 J308 · 状态门异常/回满族 —— 1426×7 + 554×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1426（args_num=2）"若对手不处于异常状态则{0}%令对手{1}"
//   554 （args_num=0）"自身处于能力下降状态时恢复全部体力"
//
// 实现口径（同形先例直系）：
//   - 1426 门 = 对手**无任何生效异常**（BattleContext::has_any_active_abnormal_status：
//     遍历官方异常槽，任一 end_round 未到期即算"处于异常"；内联函数，插件可直调）。
//     门开 → apply_anomaly_checked(对手, 异常码{1}, random_anomaly_duration(), 自己,
//     概率{0}, ChanceSource::Skill)（**J3 申报制直系**：概率申报给原语、由概率闸门
//     管道管辖；同形先例 434「若自身处于能力提升状态则{0}%令对手{1}」）。
//     门闭 → **不掷骰、不申报**（无事发生；官方模板无"未触发则"兜底子句）。
//     异常码 = 官方异常码（0麻痹 1中毒 2烧伤 3寄生对手 4寄生 5冻伤 6害怕 7疲惫
//     8睡眠 9石化 …），载体实载 {0,5,6,8,15} 全在合法集（≤43）。
//   - 554 门 = 自身任一**负槽**（六槽含命中；J144 515 的 has_any_drop 同判据）。
//     门开 → heal_max_hp_fraction(自身, 1)（= heal_amount(max_hp/1) → **回满**；
//     404「恢复双方全部体力」同款原语；吃封回血 HEAL_BLOCK 与恢复修正）。
//   - 审计槽（+owner*2）：
//       991426100 = 1426 门（0/1）；991426200 = 施加结果码（ApplyAnomalyResult；
//                    门闭记 -1 = 未申报）。
//       991554100 = 554 门（0/1）；991554200 = 实际恢复量（回满后 = 恢复上限 - 恢复前）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽、无剩余参数）：
//   1426 [概率,异常码]：28621 慈悲善念 [80,8]（cat4 属性技）、29173 利齿鲨突袭
//        [100,6]（链 [191,1875,1426]——191=免疫反弹异常，**不选**：对手会免疫）、
//        29181 钢牙极突 [100,6]、32835 雷鸣碎冰啸 [50,15]、33239 破法轰天雷 [75,0]、
//        35212 夜空翱翔 [50,6]（纯链 cat2）、**37187 寒结冰境 [100,5]**（链
//        [1426,752,1883]——**场景载体**：100% 冻伤，确定性档；752/1883 是回合末效果，
//        不污染异常/门断言）。
//   554 []：15309 满月斩（**纯链** cat1 pow140，场景载体）、17377 枯木百触斩、17461
//        光剑冲击、31094 救赎之梦、33197 厄运之刺、36835 觅浪逐涛、37828 闪击光束。
//
// 动工前置三查（2026-10-22）：①内核 grep 1426/554 无行为硬编码 ②effect_info 两条模板
//   各仅本条（无同文其它 id）③moves_lib/soul_lib 并集（jbatch/kbatch/hub 各检出）无
//   SKILL_EFFECT(1426)/SKILL_EFFECT(554)——真缺口。
//
// 场景回归：test/scenario/scenario_6410_status_gate_anomaly_heal.cpp（号段 6410-6419）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k1426GateBase = 991426100;   // +owner*2：门
constexpr int k1426ApplyBase = 991426200;  // +owner*2：施加结果码
constexpr int k554GateBase = 991554100;    // +owner*2：门
constexpr int k554HealedBase = 991554200;  // +owner*2：实际恢复量

// 自身任一负槽（六槽含命中；J144 515 同判据）。
bool j308_has_any_drop(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

void j308_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ── 1426 —— 若对手不处于异常状态则{0}%令对手{1} ──
// int_args = [owner, target, 概率, 异常码]（args_num=2）。
EffectResult effect_1426_no_anomaly_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1426, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1426, args,
                     "1426 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1426,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0, 1426, args, "需要 概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1426, args,
                     "异常码越界（官方全集 ≤43）");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (ctx->has_any_active_abnormal_status(opp)) {
        // 门闭：对手已处于异常状态 → 不掷骰、不申报（模板无兜底子句）。
        j308_set_storage(ctx, k1426GateBase + owner * 2, 0);
        j308_set_storage(ctx, k1426ApplyBase + owner * 2, -1);
        return EffectResult::kOk;
    }
    j308_set_storage(ctx, k1426GateBase + owner * 2, 1);
    const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    j308_set_storage(ctx, k1426ApplyBase + owner * 2, static_cast<int>(res));
    return EffectResult::kOk;
}

// ── 554 —— 自身处于能力下降状态时恢复全部体力 ──
// int_args = [owner, target]（args_num=0，loader 注入 owner/target）。
EffectResult effect_554_drop_state_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 554, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 554, args,
                     "554 布局=[owner,target]（args_num=0）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 554, args, "owner 越界");
    if (!j308_has_any_drop(ctx, owner)) {
        j308_set_storage(ctx, k554GateBase + owner * 2, 0);   // 门闭：不回满
        j308_set_storage(ctx, k554HealedBase + owner * 2, 0);
        return EffectResult::kOk;
    }
    j308_set_storage(ctx, k554GateBase + owner * 2, 1);
    const int before = ctx->getPet(owner).hp;
    (void)heal_max_hp_fraction(ctx, owner, 1);   // 恢复全部体力
    j308_set_storage(ctx, k554HealedBase + owner * 2, ctx->getPet(owner).hp - before);
    return EffectResult::kOk;
}

SKILL_EFFECT(1426, effect_1426_no_anomaly_chance_anomaly)
SKILL_EFFECT(554, effect_554_drop_state_full_heal)
