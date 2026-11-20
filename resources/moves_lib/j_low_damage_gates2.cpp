// J 组 J163 · 阈值响应族三 —— 949×10 / 1041×10（2026-10-04，worktree ../agents/j163）
//
// 家族语义（effect_info 模板 + 4399 载体逐字，⚠️ 1041 双语义分叉见下）：
//   949 （args_num=1）"造成伤害不足{0}则恢复自身全部体力"
//     —— 炫宇清波 30062 [280]（特130）、舍身重斩 31366 [300]（物130，纯链）。
//   1041（args_num=2）"造成的攻击伤害若低于{0}则令对手下{1}次使用的攻击技能无效"
//     —— ⚠️ **同 id 双语义分叉**（10 载体 4399 逐字核对 2026-10-04）：
//        · A 组 6 载体（多数）：「造成的伤害不足{0}则**免疫下1次受到的攻击**」
//          （幽影灭袭/残影绝尘断/灵魂收割者/耀天化冰心/冥界破魔剑/飞尘绝命弹）；
//        · B 组 4 载体（模板同文）：「令对手下1次使用的攻击技能无效」
//          （机能修正锤/神风烈冲击/寞路羔羊的追悔/奔雷极光斩）。
//        无数据位区分两组（切片同为 [阈值,1]）→ 按 869 先例（图鉴胜于陈旧模板）
//        落地**多数派 A**（自身免疫下 1 次受到的攻击 = DAMAGE counts=1 票，972/1293
//        可穿形直系）；B 组 4 载体记档挂账（R/白天拍板：同 id 双语义需逐载体
//        override 才能各归各位）。
//
// 实现口径：
//   - 949 = DETECT 攻方桶 final **严格 <** {0} → heal_max_hp_fraction(1)（775/J25 回满
//     直系；「不足」严格低于口径）。
//   - 1041(A) = DETECT final < {0} → grant_immunity(DAMAGE, coverage_all, counts={1})
//     （972「免疫下{0}次受到的攻击」直系：DAMAGE 票消费在管线 BLOCK 阶段）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   949  [阈值]：30062 [280]、31366 [300]（纯链×2 场景利）…
//   1041 [阈值,次数]：30234 残影绝尘断 [280,1]（1052 已实现=消强门，对手无提升→良性）
//   场景载体动工时按链上兄弟扫描选纯链/良性（工单行：ugrep 别名教训，纯 Python 扫描）。
//
// 动工前置三查（2026-10-04）：①内核 grep 949/1041 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2540_low_damage_gates2.cpp（号段 2540-2549）。

#include "common.h"

namespace {

// ── 949 —— 造成伤害不足{0}则恢复自身全部体力 ──
// int_args = [owner, target, 阈值]（args_num=1）。
EffectResult effect_949_low_damage_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 949, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 949, args,
                     "949 布局=[owner,target,阈值]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     949, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0, 949, args, "需要 阈值 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            if (d.final >= threshold) {
                return;   // 「不足{0}」严格低于
            }
            (void)heal_max_hp_fraction(c, owner, 1);   // 回满
        });
    return EffectResult::kOk;
}

// ── 1041(A) —— 造成的攻击伤害若低于{0}则免疫下{1}次受到的攻击（多数派语义）──
// int_args = [owner, target, 阈值, 次数]（args_num=2）。
EffectResult effect_1041_low_damage_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1041, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1041, args,
                     "1041 布局=[owner,target,阈值,次数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int counts = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1041, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && counts > 0, 1041, args, "需要 阈值/次数 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, counts](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            if (d.final >= threshold) {
                return;   // 「低于{0}」严格低于
            }
            c->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                              /*duration_rounds=*/0, /*source_id=*/0, /*soul_immunity=*/false,
                              EffectScope::ON_STAGE, /*counts=*/counts,
                              /*source_effect_id=*/1041);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(949, effect_949_low_damage_full_heal)
SKILL_EFFECT(1041, effect_1041_low_damage_immune)
