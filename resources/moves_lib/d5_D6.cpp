// D6 · 帝皇之钺 3404 第五技「黎明破云钺 30937」缺失效果 1158、1178
//（夜跑 night-2，2026-09-28）
//
// 30937 链 = [913, 1158, 1178]、args [100, 1, 50]：
//   913=[100]（消除回合类成功→下回合对手受伤+100%，effect_batch8.cpp 已接，非本单）；
//   1158=[1,50]、1178=[]（本文件）。
// 4399 图鉴（黎明破云钺）："必中；消除对手回合类效果，消除成功则对手下回合受到的伤害
//   提升100%；自身每处于1种能力强化状态则附加50点固定伤害，遇到天敌时效果翻倍；
//   自身不处于能力提升状态时造成的伤害翻倍"。
//
// 1158 "自身每处于{0}种能力提升状态则附加{1}点固定伤害，遇到天敌时效果翻倍"：
//   - 计数 = 962/982 同判据（ability_levels 6 槽正等级、含命中，用户 2026-09-18 口径）；
//   - 固定量 = kinds/per*points（1845 "每{0}点…{1}点"同式；per=1 即 kinds×50）；
//   - 天敌翻倍走 is_predator（common.h 助手：对手固有属性克制自身，464/610 同款）；
//   - 落点 SKILL_EFFECT 直发固定伤害（402 同款 deal_pink_damage/FIXED，粉伤不进攻击管线）。
// 1178 "自身不处于能力提升状态则造成的伤害翻倍"：
//   - 条件 = 1158 计数的精确补集（kinds==0，负等级不算提升）；
//   - "伤害翻倍" = 通用增伤 +100%（ws.damage_add_pct，AMP 阶段加法通道，1959 同款）；
//     AMP 在 raw_attack_damage 台账之后 → 台账记的是翻倍前的裸伤（场景按 2× 断言）。
//
// 场景回归：test/scenario/scenario_220_dihuang_3404_1158_1178.cpp。

#include "common.h"

namespace {

// "自身处于一种能力提升状态"计数（962 同判据：本体正等级、**含命中**共 6 槽，
// 用户 2026-09-18 口径；batch_a_skills.cpp 的 count_boost_kinds 同款，插件不跨 .so 复制）。
int count_boost_kinds_3404(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 1158 —— 每处于{0}种能力提升状态附加{1}点固定伤害，遇天敌翻倍 ──
EffectResult effect_1158_boost_scaled_fixed(BattleContext* ctx, const EffectArgs& args) {
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
    const int kinds = count_boost_kinds_3404(ctx, owner);
    int amount = kinds / per * points;
    if (amount <= 0) {
        return EffectResult::kOk;   // 无提升状态：不附加
    }
    if (is_predator(ctx, owner)) {
        amount *= 2;   // 遇到天敌时效果翻倍
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 1178 —— 自身不处于能力提升状态则造成的伤害翻倍 ──
EffectResult effect_1178_no_boost_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (count_boost_kinds_3404(ctx, owner) > 0) {
        return EffectResult::kOk;   // 处于任一能力提升：不翻倍
    }
    ctx->ws.damage_add_pct[owner] += 100.0f;   // 翻倍 = 通用增伤 +100%（AMP 加法通道）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1158, effect_1158_boost_scaled_fixed);
SKILL_EFFECT(1178, effect_1178_no_boost_double);
