// K253 对手能力下降→冻伤族 —— effect 336（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 336，2026-10-20）：
//   336 深海女武神 2687 args[3,5]：「对手处于能力下降状态时，则对手冻伤」
//     → db[0]/db[1] 语义存疑不消费（记档）：tips 里既无概率也无时长数值，
//       337（2683 args[2,5,100]「若对手处于冻伤状态，则自身本回合受到的伤害
//       减少100点」）等同批行的位置含义也不自洽——按 K2 纪律**解析不了宁可不用、
//       不猜**。冻伤异常码与持续回合数由 tips 承载。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   门 = 对手任一能力槽 `ability_levels[opp][i] < 0`（"处于能力下降状态"——
//     引擎口径 = 存在负等级；k165_gate_attach_power 94 / k206_cond_heal 25
//     同款判据）。
//   时点 = BATTLE_ROUND_START（回合开始时）：能力下降是**持续态**（不是"本回合
//     变化过"那种瞬时态），回合开始读 ability_levels 稳定可靠；ability_levels
//     在 stat_drop/stat_change 落点即时写入（battle_primitives.cpp 的
//     write_ability_level 唯一落点），回合边界不清零 → 持续态跨回合可见。
//   效果 = apply_anomaly(冻伤 Frostbite=5)，时长 random_anomaly_duration()
//     （= 2 回合，inline 恒定值——确定性可断言）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-20）：①内核 grep 336 无魂印行为硬编码
//    （唯一命中是 include/effects/continuousEffect.h:40 的注释"约 287 条"，
//    与本 id 无关）；②effect_icon kind=[]、tips 为权威文（dump 原文已核，
//    effect_info 无 336 技能模板）；③registry soul 集合无 336、soul_lib 全库
//    grep 无同 id 注册——真缺口。
// 先例：k165_gate_attach_power（能力下降门判据）、k206_cond_heal（条件+能力
//   下降读法）、mortarion_4541（"对手处于能力下降则令对手害怕"同构分支）、
//   k193_cond_freeze（冻伤施加通道）。
// 场景回归：test/scenario/scenario_4090_statdrop_frostbite_336.cpp（号段 4090-4099）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 336;
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

// 对手是否处于能力下降状态（引擎口径 = 任一能力槽为负等级）。
bool opp_has_stat_drop(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// ROUND_START：对手处于能力下降状态 → 令对手冻伤。
EffectResult k253_336_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!opp_has_stat_drop(ctx, opp)) {
        return EffectResult::kOk;   // 门闭：对手无能力下降
    }
    (void)g_core->apply_anomaly(ctx, opp, kFrostbite, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（336 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k253_336_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
