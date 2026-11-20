// K304 必中族 —— effect 1738（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 1738，2026-10-21）：
//   1738 拂晓兔 4150 args[0,0]（kind=[1]）：「以爱的名义，使自身所有技能必定命中！」
//     → **零参行**（args 全 0，无数值子句）→ 行参不消费（记档）。
//     ⚠️ "以爱的名义"是修辞前缀，不带机制信息；机制子句只有"所有技能必定命中"。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   零参行 → 不读行参（K224 257 首回合必中同款：args[] 空）。
//   ⚠️ 走 db_at 而非 a.row 的理由同族统一（K257 起的夜跑口径）：a.row 在**真实对局**
//      里是空的（bind_soulmark_args 丢弃 owned_int_args 前两位）→ 存量用 a.row_or 的
//      文件全靠 fallback 硬编码兜底。本单零参，两种读法等价，但**不消费任何下标**。
//
//   效果 = ws.must_hit_grant[owner] += 1（**真正的必中凭证**——skills.cpp:314：
//     `cred.must_hit = skill.must_hit || ws.must_hit_grant[owner] || force_execute`，
//     必中 = **跳过命中率掷骰**）。
//     ⚠️ **不要用 ws.accuracy_fix_grant**（那个是"基础命中率钉 100"通道，
//        battleWorkspace.h:288-293 明确：它**≠ 必中凭证**——弱化负档照样乘算
//        → 仍可能 Miss、必定闪避词条无效、失明按必中 55 开。官方"必定命中"是
//        跳过掷骰，故取 must_hit_grant）。
//   时点 = BATTLE_FIRST_MOVE_RIGHT（**FMR 桶**——K224 257 直系同款：ws 每回合 reset
//     → MOVE_RIGHT 是"reset 之后、出手之前"的唯一正确写入窗口；写在别处会被
//     ROUND_START 的 ws reset 冲掉，battleWorkspace.h:284-286 明写）。
//     ⚠️ **每回合写一次**（常驻必中，非首回合限定）：FMR 每回合都跑 → 每回合重授。
//   ⚠️ 次数型（+= 1）：ws.must_hit_grant 是次数（每回合 reset 故实际恒为 1，
//     但按次数型语义累加更安全——K224 257 同款写法）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 1738 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核；effect_info 无 1738
//    技能模板）；③registry soul 集合无 1738、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k224_firsthit_power 257（**FMR 桶 + ws.must_hit_grant += 1，直系同款**
//   ——本单是它的"非首回合限定"版）、shenjue_mistike_4676（必中凭证同款）。
// 场景回归：test/scenario/scenario_4600_must_hit_1738.cpp（号段 4600-4609）。

#include "common.h"

namespace {

constexpr int kMarkId = 1738;

// FMR 桶：每回合授予必中凭证（ws 每回合 reset → 每回合重授）。
EffectResult k304_1738_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ctx->ws.must_hit_grant[owner] += 1;   // 必中凭证（跳过命中率掷骰）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1738 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k304_1738_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
