// D8 · 混沌魔君索伦森 3414 第五技「混沌灭世决 30995」缺失效果 1133
//（夜跑 night-2，2026-09-28）
//
// 30995 链 = [1217, 1218, 1133]、args [2, 2, 2, 1, 120]：
//   1217=[2]（selection_seal.cpp 已接）、1218=[2,2]（kongyuan_skills.cpp 已接）、
//   1133=[1,120]（本文件）。
// 4399 图鉴（混沌灭世决）："必中；消除对手能力提升状态，消除成功对手下2次攻击技能无效；
//   未击败对手则下2回合自身先制+2；对手每有1项能力等级与自身相同则附加120点固定伤害"。
//
// 1133 "对手每有{0}项能力等级与自身相同则附加{1}点固定伤害"：
//   - 逐槽比对 ability_levels（含命中共 6 槽，用户 2026-09-18"等级一律指 6 种"口径），
//     数 opponent[i] == self[i] 的槽数；固定量 = matches/per*points（1845 同式）；
//   - ⚠️ 口径记档（待拍板）：**0==0 也算"相同"**（模板字面：等级相同，未排除无等级态；
//     双方全零时基准即 6×120=720 固伤——字面读法强度很高，若实测翻案只改 count 循环，
//     把"双方都为 0 的槽"剔出计数即可）；只算"双方非零且相等"的备选口径见场景注释。
//   - ⚠️ 链内顺序语义（附录二：数据序=执行序）：1133 排在 1217（消强）之后 → 比对的
//     是**消强后**的对手等级。本实现按 SKILL_EFFECT 链序自然读到后值，场景 T3 锁死。
//
// 场景回归：test/scenario/scenario_240_sorensen_3414_1133.cpp。

#include "common.h"

namespace {

// ── 1133 —— 对手每有{0}项能力等级与自身相同则附加{1}点固定伤害 ──
EffectResult effect_1133_level_match_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per = args.int_args[2];
    const int points = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || per <= 0 || points <= 0) {
        return EffectResult::kOk;
    }
    int matches = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] == ctx->ability_levels[target][i]) {
            ++matches;
        }
    }
    const int amount = matches / per * points;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1133, effect_1133_level_match_fixed);
