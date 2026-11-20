// D 组批产 · D5（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   艾丽娅 3403 第五技 侍君天尽头 30931（4399 图鉴名"待君天尽头"）缺失效果：
//     959  消除对手回合类效果，消除成功则自身全属性+{0}
//          30931 携带 [959,582] 切片 [1,2] → 959 取 [1]；同模板泛用效果，全库携带者
//          （毁灭之瞳 19686、破釜一战 19822、无极灾变 30026 等）均切 [1,*]。
//   同技能另一子句 582（未击败对手→下2回合必暴）已由 effect_batch8.cpp 实现，不重复。
//
// 实现——880（消回合→下回合先制+3，batch_b_skills.cpp）同族，奖励段不同：
//   · break_round_effects(ctx, opp) 的返回值分叉：SUCCESS 之外的 NO_EFFECTS（对手
//     本就无回合类效果）与 IMMUNE（对手免断）都算"消除未成功"→ 直接返回。
//   · SUCCESS → all_stats_up（官方 6 槽全走 stat_change 真原语，本体/视图同步、
//     过免疫/满级钳制与既有"全属性"口径一致），**立即生效**——官方文本无"下回合"
//     延迟，异于 880 的 MOVE_RIGHT 延迟 tick。
//   · 断回合本身的成功判定/事件（EVENT_BREAK）/免断交互全在原语内，不在效果侧复刻。

#include "common.h"

namespace {

// ── 959 消回合成功 → 自身全属性+{0}；Args=[owner, target, up] ──────────────────
EffectResult effect_959_break_rounds_all_stats_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int up = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // "消除成功"门槛（NO_EFFECTS/IMMUNE 都不加）
    }
    all_stats_up(ctx, owner, up);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(959, effect_959_break_rounds_all_stats_up);
