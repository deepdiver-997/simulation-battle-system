// K221 受击闪避族 —— effect 307（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 307，2026-10-07）：
//   307 贝迪恩斯 2609 args[20, 4]：「20%使对手攻击miss」
//     → args 语义存疑不消费（记档）；20=概率由 tips 承载。
//
// ⚠️ 实现形态（T4 类型化行参）：ROUND_START 节点（harness 高闪避魂印同款，K163 261
//    同槽位）——ws.dodge_rate[owner] = 0.2（闪避率=概率语义；ws 轮界自清 → 每回合
//    重注；STAGE 作用域）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 307 魂印行为硬编码 ②effect_info
//    无 307 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：scenario_harness 高闪避魂印（dodge_rate=1.0 形）、k_open_window_immune 261。
// 场景回归：test/scenario/scenario_3630_dodge.cpp（号段 3630-3639）。

#include "common.h"

namespace {

constexpr int kMarkId = 307;

EffectResult k221_307_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[owner] = 0.2f;   // 20% 闪避（每回合重注）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（307 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k221_307_round_start)});
