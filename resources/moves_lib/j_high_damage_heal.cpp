// J 组 J188 · 高伤回血族 —— 1 宏/10 载体（2026-10-22，worktree ../agents/j188）
//
// 家族语义（effect_info 模板即权威）：
//   1341 [2] 造成的伤害高于{0}则恢复自身最大体力的1/{1}
//
// 实现口径（J125 541 低伤门直系，比较方向取反）：
//   - DETECT 攻方桶（register_damage_effect(DETECT, owner, DETECT)）读管线后
//     resolvedDamage：自卡 attackerId==owner / defenderId==target / isRed / final>0。
//   - final **严格 >** {0} → heal_max_hp_fraction(owner, {1})（"高于"严格读法；
//     541 的"低于"用严格 < 同源）。
//   - 走伤害管线 DETECT 相位而非 AFTER_ACTION：阈值在本次攻击结算内即可知，
//     不依赖内核 effect_register_state 路由，与 665/1104 的 AFTER_ACTION 形并存
//     （两条通道都在既有设施内）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1341 [阈值,分母]：32505 菇菇发之梦 [300,3]（纯链，物 150）、35396 月湖倾撒 [300,3]、
//        35858 黄土飞石 [300,3]、31985 超数码星阵 链尾 [300,3]…（10/10 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1341 无硬编码（仅 K300 注释）②
// custom_effect_overrides/programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册
// ——真缺口。
// 场景回归：test/scenario/scenario_5030_high_damage_heal.cpp（号段 5030-5039）。

#include "common.h"

namespace {

// ── 1341 —— 造成的伤害高于{0}则恢复自身最大体力的1/{1} ──
// int_args = [owner, target, 阈值, 分母]（args_num=2）。
EffectResult effect_1341_high_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1341, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1341, args,
                     "1341 布局=[owner,target,阈值,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1341, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && denom > 0, 1341, args, "需要 阈值/分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤判定
            }
            if (d.final <= threshold) {
                return;   // "造成的伤害高于{0}"（严格大于）
            }
            (void)heal_max_hp_fraction(c, owner, denom);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1341, effect_1341_high_damage_heal)
