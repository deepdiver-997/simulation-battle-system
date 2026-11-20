#include <utils/sbs_rand.h>
// J 组 J40 · 旧版输出窗族 —— 3 宏/59 载体（2026-10-02，worktree ../agents/j40）
//
// 家族语义（effect_info 模板即权威）：
//   1589 [1] 若对手处于异常状态则令对手随机{0}项技能PP值归零
//   1522 [2] 附加{0}点固定伤害，若当回合造成的伤害高于{1}则固伤效果翻倍
//   708  [2] 恢复自身{0}点体力，自身体力少于1/{1}时恢复效果翻倍
//
// 实现口径：
//   - 1589 = has_any_active_abnormal_status(target) 门 → 随机 {0} 槽 PP 归零
//     （batch_b_zero_random_pp_slots 克隆：Fisher-Yates 洗牌 + pp_zero_slot 原语，
//     J34 656 同构、去概率门）。
//   - 1522 = DETECT 攻方桶（729 形）读本次红伤 final **严格 >** {1} → {0}×2，
//     否则 {0}；当回合内 deal_fixed（fixed_damage 原语，1170 口径）——DETECT 时
//     红伤未扣血（1236 注），固伤先落与红伤同回合结算，DamageSnapshot 生命周期
//     不受影响（pink/fixed 走独立原语通道，J15 1024 同款在 DETECT 内直打）。
//   - 708 = heal_amount(low ? 2×{0} : {0})（707 点数版；低体判 = hp < max/{1}，
//     max 读 numericalBase；"恢复效果翻倍" = 恢复量 ×2，heal 钳满）。
//
// 载体切片（unity 库 json_each 精确匹配 20+19+20=59；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   1589 [项数]：36945 噬界荒灵咒 [4]（特240）
//   1522 [固伤,阈值]：37503 祸肆·摧朽 [220,300]（物150）
//   708  [点数,低体分母]：27459 惬意时光 [250,2]、29385 白貌行道 [400,2]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1250_legacy_output_windows.cpp（号段 1250-1259）。

#include "common.h"

#include <cstdlib>
#include <vector>

namespace {

// 随机挑 want 个不同技能槽做 PP 归零（batch_b_zero_random_pp_slots 克隆：Fisher-Yates）。
void j40_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return;
    }
    std::vector<int> slots{0, 1, 2, 3, 4};
    const int n = std::min(want, static_cast<int>(slots.size()));
    for (int i = 0; i < n; ++i) {
        const int j = i + (sbs_rand() % (static_cast<int>(slots.size()) - i));
        std::swap(slots[static_cast<std::size_t>(i)], slots[static_cast<std::size_t>(j)]);
        (void)g_core->pp_zero_slot(ctx, target, slots[static_cast<std::size_t>(i)], actor);
    }
}

// ── 1589 —— 若对手处于异常状态则令对手随机{0}项技能PP值归零 ──
// int_args = [owner, target, slots]（args_num=1）。
EffectResult effect_1589_anomaly_gate_random_pp_zero(BattleContext* ctx,
                                                     const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1589, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1589, args,
                     "1589 布局=[owner,target,项数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slots = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1589, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(slots > 0, 1589, args, "需要 项数 > 0");
    if (!ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 门：对手处于异常
    }
    j40_zero_random_pp_slots(ctx, target, slots, owner);
    return EffectResult::kOk;
}

// ── 1522 —— 附加{0}点固定伤害，若当回合造成的伤害高于{1}则固伤效果翻倍 ──
// int_args = [owner, target, fixed, threshold]（args_num=2）。
EffectResult effect_1522_fixed_double_on_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1522, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1522, args,
                     "1522 布局=[owner,target,固伤,阈值]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1522, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0 && threshold > 0, 1522, args, "需要 固伤/阈值 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    // DETECT 攻方桶：本次红伤已算出（未扣血）——"当回合造成的伤害"即本次 final。
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, fixed, threshold](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤附加
            }
            const int amount = (d.final > threshold) ? fixed * 2 : fixed;
            (void)g_core->fixed_damage(c, target, amount);
        });
    return EffectResult::kOk;
}

// ── 708 —— 恢复自身{0}点体力，自身体力少于1/{1}时恢复效果翻倍 ──
// int_args = [owner, target, points, low_denom]（args_num=2）。
EffectResult effect_708_heal_low_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 708, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 708, args,
                     "708 布局=[owner,target,点数,低体分母]");
    const int self = args.int_args[0];
    const int points = args.int_args[2];
    const int low_denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     708, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0 && low_denom > 0, 708, args, "需要 点数/低体分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < max_hp_of(ctx, self) / low_denom;
    (void)g_core->heal_amount(ctx, self, low ? points * 2 : points);   // heal 钳满
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1589, effect_1589_anomaly_gate_random_pp_zero)
SKILL_EFFECT(1522, effect_1522_fixed_double_on_damage)
SKILL_EFFECT(708, effect_708_heal_low_double)
