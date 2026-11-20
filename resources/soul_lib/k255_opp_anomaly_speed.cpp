// K255 对手异常→速度+1族 —— effect 320（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 320，2026-10-20）：
//   320 魔化克雷斯 2660 args[2,1,1,4,1]（kind=[11]）：「对手处于异常状态时则自身速度+1」
//     → **args 布局不消费（记档）**：K4 头注定版「k_allowed 布局互不通用——行≠宏本义」
//       （636 是 (slot,delta)×count、641 是 (delta,slot)×count、642 是 code 表——
//       同族三种布局并存）。本行 5 位试解皆不自洽：641 布局解作 (delta,slot)=(1,1)(1,4)
//       得"攻击+1、速度+1"，但 tips 只说速度+1（攻击+1 无对应文本）；642 布局解作
//       (code,delta) 时 code=1 重复、自相矛盾。故按 K2 纪律**解析不了宁可不用、不猜**——
//       数值与槽位取 tips 行文（速度+1 → 槽 4 / 等级 +1）。db[3]=4（=速度槽）、db[4]=1
//       （=+1）与行文自洽，构成**弱印证**（不作消费依据，仅记档备查）。
//
// ⚠️ 实现形态（T4 类型化行参）：
//   门 = 对手处于异常状态 → `ctx->has_any_active_abnormal_status(opp)`（现代异常通道
//     的"任意活跃异常"查询面；k239_burn_drain_anomaly_mustcrit 的全池扫同款口径）。
//     注意与 336/299 的区别：那两单门是**特定**异常码（能力下降 / 害怕），本单是
//     **任意**异常——不限码、不限控制/弱化类。
//   时点 = BATTLE_ROUND_START（回合开始时）："处于异常状态"是**持续态**（不是
//     "本回合被命中过"那种瞬时态）；回合开始读异常状态稳定可靠。
//   效果 = stat_change(owner, 槽 4=速度, +1)（自身增益通道，**不查免弱**——
//     stat_drop 才查免弱；k196_round_stat_moves 390 同款自增益写法）。
//     ⚠️ 等级上限 ±6 由原语内钳。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-20）：①内核 grep 320 无魂印行为硬编码；
//    ②effect_icon kind=[11]、tips 为权威文（dump 原文已核，effect_info 无 320
//    技能模板）；③registry soul 集合无 320、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k196_round_stat_moves 390（ROUND_START 回合能力变化 + stat_change 自增益
//   通道）、k_round_stat 636/641/642（同族布局互不通用的定版依据）、
//   k239（has_any_active_abnormal_status 全池扫口径）。
// 场景回归：test/scenario/scenario_4110_opp_anomaly_speed_320.cpp（号段 4110-4119）。

#include "common.h"

namespace {

constexpr int kMarkId = 320;
constexpr int kSpeedSlot = 4;   // 能力等级槽：0攻 1特攻 2防 3特防 4速 5命中

// ROUND_START：对手处于异常状态 → 自身速度+1。
EffectResult k255_320_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;   // 门闭：对手不处于异常状态
    }
    (void)g_core->stat_change(ctx, owner, kSpeedSlot, /*delta=*/1);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（320 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k255_320_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
