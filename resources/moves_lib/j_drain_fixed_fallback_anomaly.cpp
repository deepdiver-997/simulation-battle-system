// J 组 J224 · 固定伤害未落地异常族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1895 [3] 吸取对手{0}点体力，若对手未受到固定伤害则{1}%令对手{2}
//
// 实现口径（直系先例）：
//   - 吸取 = fixed_damage(对手, {0}) + heal_amount(自身, 实际扣减量)——canglan 581
//     同款"固伤落地后按实际扣血回"（drain_hp_amount 未暴露；封回血/护罩吸收天然生效）。
//   - "未受到固定伤害" = 本次固伤实际 hp 扣减 <= 0（免粉/IMMUNE 相挡下、护罩吸收、
//     {0}<=0）。此时才申报 {1}% 异常：apply_anomaly_checked(chance=100)（1030/2558
//     申报制；{2} 异常码直读，不走文本 parser）。
//   - pct 在位即消费：roll_percent({1}) 直调（0 档天然 false、100 档短路），不用 >0 守卫。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 9/9 自洽）：1895 [amount,pct,anomaly]：
//   36302 彼空·真理重塑 [300,100,6]（场景载体；链兄弟 1896 解析/984 先制/1811 免断，
//   均无 hp/异常面）/ 36382 [300,100,6] / 36538 [300,100,15] / 36827 [150,100,28] /
//   38027 [250,100,7] / 38114 [100,100,15] / 38356 [300,100,6] / 28701 [300,100,23] /
//   28869 [300,100,0]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1895 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//   注：1895 在 boss_invalid_effects.json 清单内（boss 对局由装配期 DisabledEffects
//   自动摘除，插件不另加手动门）。
//
// 场景回归：test/scenario/scenario_5540_drain_fixed_fallback_anomaly.cpp（号段 5540-5549）。

#include "common.h"

namespace {

// ── 1895 —— 吸取{0}点体力；未受到固定伤害则{1}%令对手{2} ──
// int_args = [owner, target, amount, pct, anomaly]（args_num=3）。
EffectResult effect_1895_drain_fixed_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1895, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1895, args,
                     "1895 布局=[owner,target,amount,pct,anomaly]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int pct = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1895, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1895, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    // 吸取半：固伤 + 按实际扣减回血（免粉/护罩吸收 → 实扣 0 → 回血 0）。
    int dealt = 0;
    if (amount > 0) {
        ElfPet& foe = ctx->getPet(target);
        const int before = foe.hp;
        (void)g_core->fixed_damage(ctx, target, amount);
        dealt = before - foe.hp;
        if (dealt > 0 && g_core->heal_amount) {
            (void)g_core->heal_amount(ctx, owner, dealt);
        }
    }
    // 兜底半：未受到固定伤害才申报；pct 在位即消费（0/100 档原语内处理）。
    if (dealt <= 0 && g_core->apply_anomaly_checked
        && roll_skill_percent(*ctx, 1895, 0, owner, target, pct)) {
        (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1895, effect_1895_drain_fixed_then_anomaly)
