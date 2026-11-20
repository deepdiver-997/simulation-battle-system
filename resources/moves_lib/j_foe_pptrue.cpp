// J 组 J299 · 损PP固伤转真族 —— 1668×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1668（args_num=1）"附加对手当前已损失技能PP值总和×{0}的固定伤害，
//   若对手未受到固定伤害则额外附加等量的真实伤害"
//
// 实现口径（直系先例）：
//   - 损 PP 口径 = **对手**当前已损失技能 PP 总和（不是自身——1737 读自身，本条翻转；
//     `pp < 0` / `maxPP < 0` = 无限 PP 槽，跳过）。
//   - 第一段 = deal_pink_damage(target, lost_pp×{0}, FIXED)（粉伤即时结算）。
//   - 转真判定 = signature_true_damage.cpp 1737 **免疫型**：
//     `resolvedPink.target == target && final <= 0 && absorbed == 0` → 额外
//     deal_true_damage(target, r.raw)（等量）；护罩吃光（absorbed>0）不算免疫、不转真。
//   - 行参在位即消费：{0} 显式 0/负 = 不结算（不退默认系数）。
//   - 审计槽：991668100+owner = 对手已损失 PP 总和；991668101+owner = 固定伤害量；
//     991668102+owner = 转真量（0 = 未转）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽）：
//   **m36686 暴雨倾泻 [1114,1668]|[2,25] → 1668=[25]（场景载体；1114 在对手无
//   回合类效果时静默）** / m34895 瑰·极曜螺旋 → [30] / m35687 哀·魂归四方 → [30] /
//   m37398 哀·逆命无悔 → [30] / m37560 海啸·高能射线 → [30] / m38324 生机衰弱波 → [15] /
//   m38389 坠日贯杀 → [10]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1668 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1668)——真缺口。
//
// 场景回归：test/scenario/scenario_6320_foe_pptrue.cpp（号段 6320-6329）。

#include "common.h"

namespace {

constexpr int kLostPpAuditBase = 991668100;   // +owner：对手已损失 PP 总和
constexpr int kFixedAuditBase = 991668101;    // +owner：固定伤害量
constexpr int kTrueAuditBase = 991668102;     // +owner：转真量

void j299_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ── 1668 —— 对手已损失 PP 总和×{0} 固定伤害；未受固定伤害则等量真伤 ──
// int_args = [owner, target, coef]（args_num=1）。
EffectResult effect_1668_foe_lost_pp_fixed_true(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1668, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1668, args,
                     "1668 布局=[owner,target,系数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int coef = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1668, args, "需要 owner/target ∈ {0,1}");
    // 对手当前已损失技能 PP 总和（pp<0 / maxPP<0 = 无限 PP，跳过）。
    const ElfPet& foe = ctx->getPet(target);
    int lost_pp = 0;
    for (const Skills& sk : foe.skills) {
        if (sk.pp < 0 || sk.maxPP < 0) {
            continue;
        }
        lost_pp += std::max(0, sk.maxPP - sk.pp);
    }
    j299_set_storage(ctx, kLostPpAuditBase + owner, lost_pp);
    // 行参在位即消费：{0} 显式 0/负 = 不结算。
    const int amount = (coef > 0) ? lost_pp * coef : 0;
    j299_set_storage(ctx, kFixedAuditBase + owner, amount);
    j299_set_storage(ctx, kTrueAuditBase + owner, 0);
    if (amount <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    // 转真判定：读**刚才那一段**的即时结算结果（1737 同款免疫型判据）。
    const PinkDamageResolved& r = ctx->resolvedPink;
    if (r.target == target && r.final <= 0 && r.absorbed == 0
        && g_core->deal_true_damage) {
        (void)g_core->deal_true_damage(ctx, target, r.raw, owner);
        j299_set_storage(ctx, kTrueAuditBase + owner, r.raw);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1668, effect_1668_foe_lost_pp_fixed_true)
