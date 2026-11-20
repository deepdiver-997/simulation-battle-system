// K254 对手害怕→烧伤族 —— effect 299（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 299，2026-10-20）：
//   299 强袭·勇气号 2594 args[]（零参，kind=[]）：「若对手处于害怕状态则烧伤」
//     → 零参行：tips 里既无概率也无时长数值 → 按 tips 字面落地（K194 619 同款
//       "模板缺失时按 tips 落地"），烧伤码与时长由文本承载。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   门 = 对手处于害怕状态 → `has_active_anomaly(opp, Fear=6)`（现代异常通道的
//     条件查询面；k193_cond_freeze / k_cond_power_family 293 同款判据）。
//   时点 = BATTLE_ROUND_START（回合开始时）："处于害怕状态"是**持续态**（不是
//     "本回合被命中过"那种瞬时态）；回合开始读异常状态稳定可靠。
//   效果 = apply_anomaly(烧伤 Burn=2)，时长 random_anomaly_duration()（= 2 回合，
//     inline 恒定值——确定性可断言）。
//   ⚠️ 烧伤是**弱化类**异常（abnormal_status_kind: Burn → Debuff），不占控制位
//     ——对手处于害怕（控制类，无法行动）时施加烧伤不受"被控方无法行动"限制，
//     引擎异常通道按 target 独立处理（k172 808 同款：门看对手状态，效果照施加）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-20）：①内核 grep 299 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核，effect_info 无 299
//    技能模板）；③registry soul 集合无 299、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k172_round_start_clear_fear（ROUND_START 门 + 害怕施加通道）、
//   k193_cond_freeze（条件异常门 + 冻伤施加）、k_cond_power_family 293
//   （has_active_anomaly 条件查询面）。
// 场景回归：test/scenario/scenario_4100_fear_burn_299.cpp（号段 4100-4109）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 299;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);      // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);      // 6

// ROUND_START：对手处于害怕状态 → 令对手烧伤。
EffectResult k254_299_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!g_core->has_active_anomaly(ctx, opp, kFear)) {
        return EffectResult::kOk;   // 门闭：对手不处于害怕状态
    }
    (void)g_core->apply_anomaly(ctx, opp, kBurn, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（299 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k254_299_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
