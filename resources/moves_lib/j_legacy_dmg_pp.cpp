#include <utils/sbs_rand.h>
// J34 · 旧版伤害/PP 族 —— 105×15 / 423×16 / 656×20（2026-10-02，worktree ../agents/j34）
//
// 家族语义（effect_info 官方模板），三条都是老模板的直球伤害/PP 操作：
//
// 105（arg_count=1）"给予对象损伤的1/{0}，会恢复自己的HP"：
//   - "损伤" = 对手体力**缺口**（max_hp_of − 当前 hp，common.h 口径）→
//     fixed_damage(对手, 缺口/{0})，按**实扣**回吸 heal_amount（870 点数吸取同构、
//     方向从对手缺反转为按对手缺；缺口 0 → 无事）。
//   - 实例：极限吸取 11957 [4]、大地回春 11801 [8]（全库 15 载体，{0}∈{2,4,8,10}）。
// 423（arg_count=0）"直接造成等同于对手防御值的固定伤害"：
//   - "防御值" = 对手**当前有效防御**（ws.getTempAbilityValue(opp, DEFENSE)——与伤害
//     公式同源读法，含能力等级视图；selection_seal 特攻读视图同纪律）。零参 →
//     bind_participants 带 [owner, target]。
//   - 实例：破甲制裁 30455、幻灵破 15417（全库 16 载体）。
// 656（arg_count=2）"{0}%的概率令对手随机{1}项技能PP值归零"：
//   - roll_percent({0}) → 随机 {1} 个不同槽 pp_zero_slot（batch_b_zero_random_pp_slots
//     逐行同构：Fisher-Yates 局部洗牌、want≥5 全清、原语内跳无限 PP/发 PP 事件）。
//   - 实例：煞影灭天击 17756 [100,1]、领主之力 23902 [60,1]（全库 20 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 105/423/656 无行为硬编码 ②effect_meta
// 无分类牵连 ③moves_lib 无既有注册——真缺口。载体数核对（EXISTS 口径）：15/16/20；
// 全量切片核验 51 载体布局零异常（105 [denom] / 423 空参 / 656 [pct,slots]）。
// 先例：870（点数吸取+回吸实扣）、max_hp_of（缺口读数）、getTempAbilityValue
//   （视图读法，selection_seal:594）、batch_b 151（随机 PP 槽助手）、1441/2076
//   （pp_zero_slot 原语）。
// 场景回归：test/scenario/scenario_1190_legacy_dmg.cpp（1190 号段，一文件三宏）。

#include "common.h"

#include <vector>

namespace {

// ── 105 —— 给予对象损伤的1/{0}，会恢复自己的HP ──
EffectResult effect_105_missing_fraction_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 105, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 105, args,
                     "布局=[owner,target,denom]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 105, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(denom > 0, 105, args, "需要 denom>0");
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int missing = max_hp_of(ctx, opp) - ctx->getPet(opp).hp;
    if (missing <= 0) {
        return EffectResult::kOk;   // 对手满血：无损伤可结算
    }
    const int dmg = missing / denom;
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, dmg);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        g_core->heal_amount(ctx, self, dealt);   // 回吸按实扣（封回血生效，2274 口径）
    }
    return EffectResult::kOk;
}

// ── 423 —— 直接造成等同于对手防御值的固定伤害（零参）──
EffectResult effect_423_def_equal_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 423, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 423, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 423, args,
                     "owner/target 越界");
    // "防御值"= 当前有效防御（含能力等级视图，与伤害公式同源——selection_seal:594）。
    const int defense =
        ctx->ws.getTempAbilityValue(opp, NumericalPropertyIndex::DEFENSE,
                                    /*pierce_boost=*/false);
    if (defense <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(ctx, opp, defense);
    return EffectResult::kOk;
}

// ── 656 —— {0}%的概率令对手随机{1}项技能PP值归零 ──
EffectResult effect_656_chance_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 656, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 656, args,
                     "布局=[owner,target,pct,slots]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int slots = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 656, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(pct > 0 && slots > 0, 656, args, "需要 pct>0 && slots>0");
    if (!g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    if (!roll_percent(pct)) {
        return EffectResult::kOk;
    }
    // batch_b_zero_random_pp_slots 逐行同构（want>=5 → 全部；随机不同槽）。
    std::vector<int> all{0, 1, 2, 3, 4};
    const int n = std::min(slots, static_cast<int>(all.size()));
    for (int i = 0; i < n; ++i) {
        const int j = i + (sbs_rand() % (static_cast<int>(all.size()) - i));
        std::swap(all[static_cast<std::size_t>(i)], all[static_cast<std::size_t>(j)]);
        (void)g_core->pp_zero_slot(ctx, opp, all[static_cast<std::size_t>(i)], self);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(105, effect_105_missing_fraction_drain)
SKILL_EFFECT(423, effect_423_def_equal_fixed)
SKILL_EFFECT(656, effect_656_chance_random_pp_zero)
