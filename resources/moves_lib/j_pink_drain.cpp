// J 组 J7 · 粉伤回吸族 —— 7 宏 / 77 载体（2026-10-01，worktree ../agents/j7）
//
// 家族语义（effect_info 模板同构 + 4399 双源）："附加自身最大体力{0}%的百分比伤害(+变体)"。
// 粉伤 = deal_pink_damage(PERCENT_VALUE, amount=max×{0}/100)（引擎 PERCENT 换算同款
// 整除）；回血 = heal_amount（吃封回血/恢复修正，原语口径）。
// ⚠️ 4399 文本多把本族写成"固定伤害"（山河破碎/天神灭世罚/后发制人诀逐字）——
// effect_info 模板"百分比伤害"即权威（PERCENT_VALUE 档），4399 措辞从宽记档。
//
// 7 宏与分支（args_num = side_effect.arg_count 全一致）：
//   831  [1] 裸粉伤 {0}%
//   839  [1] 粉伤 + 恢复等量（恢复量=名义粉伤量；粉被护罩吸收时 heal 不缩，记档）
//   852  [2] 粉伤 + 恢复等量，自身体力 < 最大体力1/{1} → **恢复与粉伤双翻**
//            （翻倍判定在动作前一次性求值：4399 山河破碎"体力小于1/3时效果翻倍"）
//   1206 [3] 粉伤 {0}%，每次使用+{1}%，最高{2}% —— 递增档存 plugin_storage
//            （键=990120600+owner；837 soulmark_storage 同风格。多载体共用一档——
//            EffectArgs 不携带技能 id，同宠不同 1206 技共享爬档，记档）
//   1697 [1] 粉伤 {0}%，对手免粉 → **额外附加**等量真伤 —— is_immune(PINK_DAMAGE)
//            预查询（zhakes 4762 同款）：免粉→粉 0 落地、改打等量真伤（净效果=真伤）
//   1648 [1] 粉伤 + 恢复等量，对手存在**致命裂痕**时转等量真伤 —— ⚠️ 记档：裂痕
//            引擎无表示（grep 全库零命中）→ 恒常径（粉+回血）；裂痕原语落地补转换支
//   1765 [1] 同 1648 但条件为**处决印记** —— 同上记档（印记无表示，恒常径）
//
// 动工前置三查（2026-10-01）：①内核 grep 7 id 无硬编码 ②custom_effect_seed.sql 零命中
// ③moves_lib 无既有注册——真缺口；载体数 77 与工单一致，纯载体 831×2、1206。
// 先例：837（递增档）、1255/1788（PERCENT_VALUE 粉伤 / is_immune 预查询）。
// 场景回归：test/scenario/scenario_920_pink_drain.cpp（920-924 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k1206StorageBase = 990120600;   // 1206 递增档键基（+owner）

// 名义粉伤量 = 自身最大体力 × pct / 100（整除，引擎 PERCENT 换算同口径）。
inline int j7_pink_amount(BattleContext* ctx, int owner, int pct) {
    return max_hp_of(ctx, owner) * pct / 100;
}

}  // namespace

// ── 831 —— 附加自身最大体力{0}%的百分比伤害 ──
EffectResult effect_831_selfhp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 831, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 831, args,
                     "831 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 831, args,
                     "需要 owner/target ∈ {0,1}");
    if (pct > 0 && g_core->deal_pink_damage && ctx->getPet(opp).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, j7_pink_amount(ctx, owner, pct),
                                       DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 839 —— 附加自身最大体力{0}%的百分比伤害并恢复等量体力 ──
EffectResult effect_839_selfhp_pink_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 839, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 839, args,
                     "839 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 839, args,
                     "需要 owner/target ∈ {0,1}");
    const int amount = j7_pink_amount(ctx, owner, pct);
    if (amount <= 0 || !g_core->deal_pink_damage || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, amount);   // 恢复等量（名义量，见头注）
    }
    return EffectResult::kOk;
}

// ── 852 —— …并恢复等量体力，自身体力低于最大体力1/{1}则恢复效果和百分比伤害翻倍 ──
EffectResult effect_852_selfhp_pink_heal_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 852, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 852, args,
                     "852 布局=[owner,target,pct,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 852, args,
                     "需要 owner/target ∈ {0,1}");
    if (pct <= 0 || !g_core->deal_pink_damage || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    // 翻倍判定一次性求值（动作前）：体力 < 最大体力 1/{denom}（严格小于）→ 双翻
    bool doubled = false;
    if (denom > 0) {
        doubled = ctx->getPet(owner).hp < max_hp_of(ctx, owner) / denom;
    }
    // 双翻 = 百分比先翻倍再取整（{pct}×2 → max×2pct/100），非"整除后×2"
    //（99999×30%=29999→×2=59998 ≠ 99999×60%=59999，场景 T922.b 钉死该口径）
    const int amount =
        max_hp_of(ctx, owner) * pct * (doubled ? 2 : 1) / 100;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, amount);   // 恢复"等量"同倍率
    }
    return EffectResult::kOk;
}

// ── 1206 —— 附加自身最大体力{0}%的百分比伤害，每次使用增加{1}%，最高{2}% ──
EffectResult effect_1206_growing_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1206, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1206, args,
                     "1206 布局=[owner,target,base,growth,cap]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base = args.int_args[2];
    const int growth = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1206, args,
                     "需要 owner/target ∈ {0,1}");
    if (base <= 0 || !g_core->deal_pink_damage || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    // 递增档：已用次数 n → 本次 pct = min(base + growth×n, cap)；用后 n+1
    const int key = k1206StorageBase + owner;
    int uses = 0;
    if (auto it = ctx->plugin_storage.find(key); it != ctx->plugin_storage.end()) {
        uses = any_int_or(it->second);
    }
    int pct = base + growth * uses;
    if (cap > 0 && pct > cap) {
        pct = cap;
    }
    ctx->plugin_storage[key] = uses + 1;
    (void)g_core->deal_pink_damage(ctx, opp, j7_pink_amount(ctx, owner, pct),
                                   DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 1697 —— …百分比伤害，若对手免疫百分比伤害则额外附加等量的真实伤害 ──
EffectResult effect_1697_pink_or_true(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1697, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1697, args,
                     "1697 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1697, args,
                     "需要 owner/target ∈ {0,1}");
    if (pct <= 0 || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    const int amount = j7_pink_amount(ctx, owner, pct);
    if (ctx->is_immune(opp, ImmunityType::PINK_DAMAGE, ctx->currentState)) {
        // 免粉 → 粉 0 落地，"额外附加"等量真伤（净效果 = 真伤，真伤穿免粉）
        if (g_core->deal_true_damage) {
            (void)g_core->deal_true_damage(ctx, opp, amount, owner);
        }
        return EffectResult::kOk;
    }
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1648 / 1765 共体 —— 粉伤+回血；条件标记（致命裂痕/处决印记）存在时转等量真伤 ──
// ⚠️ 裂痕/印记引擎均无表示 → 条件恒假、恒走常径（粉+回血）。标记原语落地后在此接
// 查询并把粉伤支改为 deal_true_damage（1648/1765 各自的转换条件互不相同）。
EffectResult j7_pink_heal_mark_cond(BattleContext* ctx, const EffectArgs& args, int effect_id) {
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int amount = j7_pink_amount(ctx, owner, pct);
    if (amount <= 0 || !g_core->deal_pink_damage || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// ── 1648 —— …并恢复等量体力值，对手存在致命裂痕时转变为等量的真实伤害 ──
EffectResult effect_1648_pink_heal_crack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1648, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1648, args,
                     "1648 布局=[owner,target,pct]");
    EFFECT_ARG_GUARD(args.int_args[0] >= 0 && args.int_args[0] <= 1 && args.int_args[1] >= 0
                         && args.int_args[1] <= 1,
                     1648, args, "需要 owner/target ∈ {0,1}");
    return j7_pink_heal_mark_cond(ctx, args, 1648);   // 裂痕恒无 → 常径（见头注记档）
}

// ── 1765 —— …并恢复等量体力值，若对手存在处决印记则转变为等量的真实伤害 ──
EffectResult effect_1765_pink_heal_mark(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1765, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1765, args,
                     "1765 布局=[owner,target,pct]");
    EFFECT_ARG_GUARD(args.int_args[0] >= 0 && args.int_args[0] <= 1 && args.int_args[1] >= 0
                         && args.int_args[1] <= 1,
                     1765, args, "需要 owner/target ∈ {0,1}");
    return j7_pink_heal_mark_cond(ctx, args, 1765);   // 印记恒无 → 常径（见头注记档）
}

SKILL_EFFECT(831, effect_831_selfhp_pink);
SKILL_EFFECT(839, effect_839_selfhp_pink_heal);
SKILL_EFFECT(852, effect_852_selfhp_pink_heal_double);
SKILL_EFFECT(1206, effect_1206_growing_pink);
SKILL_EFFECT(1697, effect_1697_pink_or_true);
SKILL_EFFECT(1648, effect_1648_pink_heal_crack);
SKILL_EFFECT(1765, effect_1765_pink_heal_mark);
