// K345 — 2433 火箭骑士君（4815），effect_icon 唯一行 2007。
// tips：回合开始时，若自身处于异常状态则将其解除（boss无效）。
// DB 原序 args=[1,0] 无可与文案互锚的数值，不猜成概率/次数；不读 a.row。
// 三查：内核无 2433 硬编码；effect_info 2433 是不同文技能模板；
// soul_lib/moves_lib 及全部 feat/* 无注册。2181 技能模板仅动作同文，时点不同。
// 套路：K206 ROUND_START 声明节点 + 极渊 dispel_active_anomalies 原语。
// 全解除走效果解除路径，不模拟自然到期，不手写异常槽或 watcher。
#include "common.h"

namespace {
constexpr int kMarkId = 2433;

EffectResult cleanse_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->dispel_active_anomalies ||
        a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    (void)g_core->dispel_active_anomalies(ctx, a.owner);
    return EffectResult::kOk;
}
}  // namespace

SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &cleanse_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
