// J185 对手降态威力提升族 —— effect 1953（1 宏 9 载体）
//
// 行清单（DB moves.side_effect 含 1953 的全载体，2026-10-07；arg_count 表 JSON
// 数组切段——⚠️ 字符串切片会切错位，必须 json.loads 后按位切）：
//   官方模板「对手处于能力下降状态则威力提升{0}%」，arg_count=1=args_num；
//   9 载体切片全部自洽：36522 偸天换日[50] / 36594 钳坤一掷[50] / 36700 微醺盎盎[50] /
//   36866 大漠狂沙烫[60] / 36949 全武装奏鸣[50] / 37436 未抵的本相[100] /
//   37709 酥麻麻冲击[50] / 38211 金雨天灵[50] / 38358 极影绘风岚[60]。
//
// 落法（J182 1227 判据反向 + batch_b 559 view 改写同路）：SKILL_EFFECT 时点
//   count_negative(opp)>0（6 槽任一负等级，962"含命中"口径反向）→
//   skill_power_view ×(100+{0})/100。门查本体 ability_levels（非视图）。
//
// 三查记录（2026-10-07）：①内核无 1953 硬编码 ②effect_info args_num=1 =
// side_effect.arg_count=1 双表一致 ③registry moves 无 1953、moves_lib 无同 id 注册。
// 场景回归：test/scenario/scenario_3660_j185_effect1953.cpp（号段 3660-3669）。

#include "common.h"

namespace {

// 对手处于能力下降状态 = 6 槽（含命中）任一负等级（962/J182 判据反向）
bool foe_has_drop(const BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

EffectResult effect_1953_dropcond_power_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (!foe_has_drop(ctx, opp)) {
        return EffectResult::kOk;   // 门关：对手无降态不加成
    }
    auto& view = ctx->ws.skill_power_view[owner];
    view = view * (100 + pct) / 100;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）────────────────────────────────────────────────────
SKILL_EFFECT(1953, effect_1953_dropcond_power_up);
