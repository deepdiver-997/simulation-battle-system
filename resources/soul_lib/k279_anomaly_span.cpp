// K182 异常时限族 —— effect 279（1 宏 2 行，零参）
//
// 行清单（tools/soul_effects_dump.py 279，2026-10-07）：
//   279：1633 缪斯 / 2527 赤瞳缪斯「所有自身受到的异常状态只持续一回合」
//       （两行同文同构、args 空）
// 实现：ROUND_START 声明节点（K2 306 同款注册形）+ onstage 门 → 幂等写
// ctx->anomaly_duration_cap[owner]=1。字段 = 476 套装线同款
// （shengmang_476.cpp:121 激时装一次 cap=2；battle_primitives.cpp:184 消费
// min(duration, cap)——施加路径统一钳制，魂印改 onstage 常驻每回合保写）；
// 字段随对局清（battleContext.h:1144）。
// ⚠️ 三查记录（2026-10-07）：①内核无 279 硬编码；②effect_icon 两行零参自洽；
// ③并集 registry 无 279 注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── effect 279：自身受到的异常状态只持续一回合（缪斯双生）──────────────────

EffectResult soulmark_279_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 279)) {
        return EffectResult::kOk;
    }
    ctx->anomaly_duration_cap[owner] = 1;   // 幂等写；下场即失写（onstage 门）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
// 279 = ROUND_START 节点（无注册副作用，不需要防重）。
SOUL_PROGRAM(279, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_279_round_start)});
