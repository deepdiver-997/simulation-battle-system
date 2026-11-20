// K299 免疫能力下降族 —— effect 364（1 宏 / 1 行）
//（天狐·勾玉 2766 零参；号段 4550-4559，worktree ../agents/k299，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 364，2026-10-21）：
//   「免疫能力下降（boss无效）」→ 零参按 tips 落地：STAT_DROP 全覆盖、自身常驻。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b364\b 无魂印硬编码；②effect_info
//    364 无模板行；③soul_lib 全 worktree/registry 无 364 注册——真缺口。
// 实现（板行处方：hamo/encarlos/k_fullhp_immune 同款票；2492 圣莫 enter+ROUND_START
//   双节点挂法）：grant_immunity(owner, STAT_DROP, coverage_all(), duration=1)——
//   enter 首挂（覆盖首回合，早于首个 ROUND_START）+ ROUND_START 每回合重授（ws/
//   票随上下场清 → ON_STAGE 域常驻即"在场就有"）。boss 无效 → 双节点 boss_invalid 旗。
// 注册：SOUL_PROGRAM(364) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4550_stat_drop_immune.cpp（号段 4550-4559）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 364;

// 挂票体（enter 首挂 / ROUND_START 每回合重授共用）。
EffectResult k299_364_grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(a.owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/kMarkId);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（364 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k299_364_grant,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k299_364_grant,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
