// K307 先手后手分支增益族 —— effect 843（1 宏 / 1 行）
//（炫羽 3222 args[1,0]；号段 4630-4639，worktree ../agents/k307，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 843，2026-10-21）：
//   「当回合若自身先出手，则战斗阶段结束时双防+1；当回合若自身后出手，则战斗阶段
//   结束时特攻+1、速度+1」
//   → db[0]=1 = 增量（三处"+1"字面互证）；db[1]=0 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b843\b 无魂印硬编码；②effect_info
//    843 无模板行；③soul_lib 全 worktree/registry 无 843 注册——真缺口。
// 实现（板行处方：751 直系同型 + 先手门）：
//   BATTLE_ROUND_END 桶（"战斗阶段结束"）：读当回合 ctx->preemptive_right（memset
//   在次回合 MOVE_RIGHT，SOP §二.2——ROUND_END 时当回合先手权仍在）→ 印主先手
//   → stat_change(防2,+db[0]) + (特防3,+db[0])；后手 → (特攻1,+db[0]) + (速4,+db[0])
//   （自增益通道）。pr 为 NONE/平局（无行动回合）不落分支。
// 注册：SOUL_PROGRAM(843) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4630_order_branch_boost.cpp（号段 4630-4639）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 843;

// ROUND_END：先手/后手分支增益。
EffectResult k307_843_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    int step = 1;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            step = db[0];
        }
    }
    const PreemptiveRight pr = ctx->preemptive_right;
    const bool i_am_first =
        (owner == 0 && pr == PreemptiveRight::SEER_ROBOT_1) ||
        (owner == 1 && pr == PreemptiveRight::SEER_ROBOT_2);
    const bool i_am_second =
        (owner == 0 && pr == PreemptiveRight::SEER_ROBOT_2) ||
        (owner == 1 && pr == PreemptiveRight::SEER_ROBOT_1);
    if (i_am_first) {
        (void)g_core->stat_change(ctx, owner, /*stat=*/2, step);   // 防御
        (void)g_core->stat_change(ctx, owner, /*stat=*/3, step);   // 特防
    } else if (i_am_second) {
        (void)g_core->stat_change(ctx, owner, /*stat=*/1, step);   // 特攻
        (void)g_core->stat_change(ctx, owner, /*stat=*/4, step);   // 速度
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（843 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k307_843_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
