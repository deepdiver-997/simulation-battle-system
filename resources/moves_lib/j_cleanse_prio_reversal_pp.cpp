#include <utils/sbs_rand.h>
// J 组 J105 · 消状态先制/反转PP族 —— 629×14 / 1495×13（2026-10-03，worktree ../agents/j105）
//
// 家族语义（effect_info 官方模板即权威）：
//   629（arg_count=2）"消除{0}状态，消除成功下回合自身先制+{1}"
//   1495（arg_count=2）"反转对手能力提升状态，反转成功则{0}%令对手随机{1}个技能PP值归零"
//
// 实现口径：
//   - 629 = set_abnormal_status_end_round(ctx, opp, {0}, roundCount) 即刻失活
//     （"消除"=压 end_round 到当回合，has_active 即刻为假——resistance 路径同款）；
//     消除成功门 = 施加前 has_active_anomaly(opp, {0})（CoreApi 槽）→
//     NextRounds(1) ROUND_START 先制 tick（1204 通道）。
//     ⚠️ 事件选型：不经 EVENT 面（J100 实证 SKILL_USED 入队晚、BEFORE_HIT 每回合
//     一次），直接原语操作。
//   - 1495 = stat_boost_reversal 门（J99 778 同款 == REVERSED）→ roll_percent({0})
//     → Fisher-Yates 洗牌取前 {1} 槽 pp_zero_slot（1235/1441 同款本地实现）。
//
// 载体切片（unity 库 json_each 精确匹配 14/13=27；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   629 [状态码,先制]：分散（纯链 瞬迅风牙 16327 [0,1] 物90——消除麻痹(0) +1 先制）
//   1495 [概率,槽数]：{100,2} 分散（场景载体 欺诈幻影 33860 [1495,594✓]|[100,2]——
//     594 已实现良性）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 629/1495 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：resistance 路径 set_abnormal_status_end_round（battle_primitives [4]）、
//   1204（NextRounds 先制 tick，J18）、778/1110（reversal 门）、1441（洗牌PP归零）。
// 场景回归：test/scenario/scenario_1990_cleanse_prio_reversal_pp.cpp（1990 号段）。

#include "common.h"

namespace {

// 随机挑 want 个不同技能槽 PP 归零（1235/1441 同款洗牌本地实现）。
void j105_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return;
    }
    constexpr int kN = 5;   // 技能槽固定 5（elf-pet.h std::array<Skills, 5>）
    int slots[kN] = {0, 1, 2, 3, 4};
    for (int i = kN - 1; i > 0; --i) {
        std::swap(slots[i], slots[ctx->rng().next() % (i + 1)]);
    }
    for (int k = 0; k < want && k < kN; ++k) {
        (void)g_core->pp_zero_slot(ctx, target, slots[k], actor);
    }
}

// ── 629 —— 消除{0}状态，消除成功下回合自身先制+{1} ──
// int_args = [owner, target, anomaly, prio]（args_num=2）。
EffectResult effect_629_cleanse_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 629, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 629, args,
                     "629 布局=[owner,target,状态码,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int prio = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     629, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 629, args,
                     "需要 状态码 ∈ [0,43]");
    EFFECT_ARG_GUARD(prio > 0, 629, args, "需要 先制 > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->set_abnormal_status_end_round) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, anomaly)) {
        return EffectResult::kOk;   // "消除成功"门槛：对手不处于{0}状态
    }
    // 消除：end_round 压到当回合（即刻失活，resistance 路径同款语义）。
    // 走 CoreApi 槽（BattleContext 方法非 inline，插件不可直调——击杀线教训 #4）。
    g_core->set_abnormal_status_end_round(ctx, opp, anomaly, ctx->roundCount);
    // 下回合自身先制 +{1}（1204 通道）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 629,
                                /*rounds=*/1, {owner, prio},
                                [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                                    if (!c || !a.int_args || a.int_count < 2) {
                                        return EffectResult::kOk;
                                    }
                                    c->ws.preemptive_level[a.int_args[0]] += a.int_args[1];
                                    return EffectResult::kOk;
                                });
    return EffectResult::kOk;
}

// ── 1495 —— 反转对手能力提升状态，反转成功则{0}%令对手随机{1}个技能PP值归零 ──
// int_args = [owner, target, chance, slots]（args_num=2）。
EffectResult effect_1495_reversal_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1495, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1495, args,
                     "1495 布局=[owner,target,概率,槽数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int slots = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1495, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && slots > 0, 1495, args, "需要 概率/槽数 > 0");
    if (!g_core || !g_core->stat_boost_reversal || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, opp) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // "反转成功"门槛
    }
    if (roll_percent(ctx->rng(), chance)) {
        j105_zero_random_pp_slots(ctx, opp, slots, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(629, effect_629_cleanse_prio)
SKILL_EFFECT(1495, effect_1495_reversal_pp_zero)
