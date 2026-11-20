// J 组 J290 · 体力基准粉伤族 —— 3 宏/20 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1229 [1] 自身体力高于对手时附加自身当前体力1/{0}的百分比伤害
//   673  [2] 对手不处于{0}状态时附加对手已损失体力{1}%的百分比伤害
//   1045 [1] 未击败对手则附加自身最大体力1/{0}的百分比伤害
//
// 实现口径：
//   - 1229 = 体力优势门（**严格** hp > 对手当前体力）+ 当前体力 1/{0}：
//     门在 SKILL_EFFECT 时点读"本次攻击落地前"的双方体力（与官方"附加"子句一致）。
//     数值 = 自身当前体力 / {0}（整数除法，模板是"当前体力"，不是最大体力）。
//   - 673 = 非异常门 + 已损失体力%：门 = 目标**没有** {0} 号异常（has_active_anomaly，
//     只判指定状态；带别的异常不影响本门）。数值 = (最大体力 - 当前体力) × {1} / 100，
//     中间积走 int64（防止场景堆大 HP 时 int 溢出变负）。
//   - 1045 = 未击败门 AFTER_ACTION 单发（j_notdefeat_pink.cpp 1285 同门/同注册形）：
//     SKILL_EFFECT 时点注册两侧 AFTER_ACTION 桶（rounds=1，桶只跑行动方），
//     行动末对手 hp > 0 才结算；数值 = 自身最大体力 / {0}。
//   - 三条落点统一 deal_pink_damage(PERCENT_VALUE, 已算好的具体值)——模板"百分比伤害"，
//     走粉伤百分比档抗性/护罩；PERCENT_VALUE 是"值由来源算出"的口径（core_api.h 注释）。
//
// plugin_storage 审计槽（精确断言用；grep 全库无撞）：
//   991229000 + owner = 1229 本次粉伤值（门闭写 0）
//   996730000 + owner = 673 本次粉伤值（门闭写 0）
//   996730100 + owner = 673 本次"对手已损失体力"基数
//   991045000 + owner = 1045 本次粉伤值（未击败才写；门闭保持缺省 0）
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22）：
//   1229 [分母]：33517 魔水天倾 [1229]|[3]、35350 暗蛇獠牙 [1229]|[3]、35466 附芒铁臂 …
//     （7/7，分母全 3）
//   673 [状态码,百分比]：17238 极电狂龙祭 [673]|[0,25]（麻痹）、36911 星汉玄音 [673]|[5,30]
//     （冻伤）、37443 落日如滴 [96,673,964]|[2,40]…（6/6）
//   1045 [分母]：30247 暴君狂怒吼 [1045]|[3]、30718 红焰焚世诀 [781,1045]|[2,3]…（7/7，分母全 3）
//
// 动工前置三查（2026-10-22）：①内核 grep 1229/673/1045 无硬编码
//   ②custom_effect_programs/overrides 零行 ③全工作区 moves_lib/soul_lib 并集无
//   SKILL_EFFECT 注册——真缺口。
// 先例：j_hp_ratio_pink_drain.cpp 1136（当前体力分母粉伤）、
//   j_notdefeat_pink.cpp 1285（未击败门 AFTER_ACTION 单发）。
// 场景回归：test/scenario/scenario_6230_hp_basis_pink.cpp（号段 6230-6239）。

#include "common.h"

#include <cstdint>

namespace {

constexpr int k1229PinkAuditBase = 991229000;  // +owner：本次粉伤值
constexpr int k673PinkAuditBase = 996730000;   // +owner：本次粉伤值
constexpr int k673LostAuditBase = 996730100;   // +owner：本次已损失体力基数
constexpr int k1045PinkAuditBase = 991045000;  // +owner：本次粉伤值

// ── 1229 —— 自身体力高于对手时附加自身当前体力1/{0}的百分比伤害 ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1229_hp_advantage_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1229, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1229, args,
                     "1229 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1229, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1229, args, "需要 分母 > 0");
    ctx->plugin_storage[k1229PinkAuditBase + owner] = 0;
    const int self_hp = ctx->getPet(owner).hp;
    const int foe_hp = ctx->getPet(target).hp;
    if (self_hp <= foe_hp) {
        return EffectResult::kOk;   // 门：自身体力**高于**对手（严格）
    }
    const int pink = self_hp / denom;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1229PinkAuditBase + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 673 —— 对手不处于{0}状态时附加对手已损失体力{1}%的百分比伤害 ──
// int_args = [owner, target, 状态码, 百分比]（args_num=2）。
EffectResult effect_673_lost_hp_pct_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 673, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 673, args,
                     "673 布局=[owner,target,状态码,百分比]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     673, args, "需要 owner/target ∈ {0,1}");
    // 红线7：pct 在位即消费（0 = 显式关闸档，不拒绝）。
    EFFECT_ARG_GUARD(status >= 0, 673, args, "需要 状态码 >= 0");
    EFFECT_ARG_GUARD(pct >= 0, 673, args, "需要 百分比 >= 0");
    ctx->plugin_storage[k673PinkAuditBase + owner] = 0;
    const int max_hp = max_hp_of(ctx, target);
    const int lost = std::max(0, max_hp - ctx->getPet(target).hp);
    ctx->plugin_storage[k673LostAuditBase + owner] = lost;
    const bool statused = g_core && g_core->has_active_anomaly
                          && g_core->has_active_anomaly(ctx, target, status);
    if (statused) {
        return EffectResult::kOk;   // 门：对手**不处于** {0} 状态
    }
    // lost × pct / 100：int64 中间积（实际对局 hp 至多千万级，不会溢出）。
    const int pink = static_cast<int>(static_cast<std::int64_t>(lost) * pct / 100);
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k673PinkAuditBase + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1045 的 AFTER_ACTION tick —— payload={owner,target,分母}；未击败 → max/{0} 粉伤 ──
EffectResult j290_1045_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 门：当回合未击败对手
    }
    const int pink = max_hp_of(ctx, owner) / denom;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1045PinkAuditBase + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1045 —— 未击败对手则附加自身最大体力1/{0}的百分比伤害 ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1045_notdefeat_max_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1045, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1045, args,
                     "1045 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1045, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1045, args, "需要 分母 > 0");
    ctx->plugin_storage[k1045PinkAuditBase + owner] = 0;
    // 行动末判定：两侧 AFTER_ACTION 都注册，桶按行动方只跑一侧（1285 同形）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1045, /*rounds=*/1, {owner, target, denom},
                              &j290_1045_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1229, effect_1229_hp_advantage_pink)
SKILL_EFFECT(673, effect_673_lost_hp_pct_pink)
SKILL_EFFECT(1045, effect_1045_notdefeat_max_pink)
