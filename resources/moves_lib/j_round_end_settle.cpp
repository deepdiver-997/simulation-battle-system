// J 组 J50 · 随机异常/回合末结算族 —— 3 宏/57 载体（2026-10-02，worktree ../agents/j50）
//
// 家族语义（effect_info 模板即权威）：
//   176 [1] {0}%概率随机附加烧伤、冻伤、中毒、麻痹、害怕、睡眠中的一种异常状态
//   644 [1] 当回合未击败对手，则减少对手当前体力1/{0}
//   579 [1] 若当回合未击败对手，则恢复自身最大体力的1/{0}
//
// 实现口径：
//   - 176 = attach_random_anomalies（现代通道，概率门在原语内掷）+ **定制六池**
//     {烧伤2, 冻伤5, 中毒1, 麻痹0, 害怕6, 睡眠8}（count=1；1929 全池变体）。
//   - 644/579 = AFTER_ACTION 单发（双侧注册、桶只跑行动方，52 教训）：
//     "当回合未击败对手" = 行动末对手在场宠 hp>0（934 EVENT_DEATH 门的存活镜像，
//     J31 记档同源）。644：deal_pink_damage(FIXED, target 当前 hp/{0})——"当前体力"
//     读 getPet(target).hp（实扣前快照）；579：heal_max_hp_fraction(1/{0})。
//     两宏互为镜像但独立宏（19+18 载体各自持有）。
//
// 载体切片（unity 库 json_each 精确匹配 19+18+18=55→去重 57 计账口径一致；切片全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   176 [pct]：15156 玄烈炽焰击 [60]、15974 全领域沉默 [50]
//   644 [分母]：16336 武圣劈天斩 [8]、16543 毁邪灭魔斩 [4]
//   579 [分母]：15887 罗刹·谶 [5]、16572 上古破魂啸 [5]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口（attach_random_anomalies 六池
// 定制传入系 1929 既有原语）。
// 场景回归：test/scenario/scenario_1360_round_end_settle.cpp（号段 1360-1369）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 176 定制六池：烧伤/冻伤/中毒/麻痹/害怕/睡眠（模板字面六项，顺序即池序）。
inline std::vector<int> j50_pool_six() {
    return {static_cast<int>(AbnormalStatusId::Burn),      static_cast<int>(AbnormalStatusId::Frostbite),
            static_cast<int>(AbnormalStatusId::Poison),    static_cast<int>(AbnormalStatusId::Paralysis),
            static_cast<int>(AbnormalStatusId::Fear),      static_cast<int>(AbnormalStatusId::Sleep)};
}

// ── 176 —— {0}%概率随机附加六池中的一种异常状态 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_176_random_six_pool(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 176, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 176, args,
                     "176 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     176, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 176, args, "需要 pct > 0");
    if (!g_core || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    (void)g_core->attach_random_anomalies(ctx, target, j50_pool_six(), /*count=*/1, pct, owner,
                                          ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 176/644/579 共用：AFTER_ACTION 单发"未击败门"判定体（934 存活镜像）──
// 返回 true = 对手仍存活（未击败）→ 子句执行。
bool j50_not_defeated(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return false;
    }
    const int opp = args.int_args[1];
    if (opp < 0 || opp > 1) {
        return false;
    }
    return ctx->getPet(opp).hp > 0;
}

// ── 644 的 AFTER_ACTION tick —— payload={owner, target, denom}；未击败 → 削当前 1/{0}。
EffectResult j50_644_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!j50_not_defeated(ctx, args)) {
        return EffectResult::kOk;   // "当回合未击败对手"门
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (denom <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int cur = ctx->getPet(target).hp;
    const int amount = cur / denom;
    if (amount > 0 && cur > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 579 的 AFTER_ACTION tick —— payload={owner, target, denom}；未击败 → 回 1/{0}。
EffectResult j50_579_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!j50_not_defeated(ctx, args)) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    (void)args.int_args[1];
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// AFTER_ACTION 双侧单发注册（InRounds rounds=1 含当回合；桶只跑行动方，52 教训）。
void j50_register_after_action(BattleContext* ctx, int owner, int effect_id, int rounds,
                               std::vector<int> payload, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, effect_id, rounds, payload, fn);
    }
}

// ── 644 —— 当回合未击败对手，则减少对手当前体力1/{0} ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_644_not_defeat_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 644, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 644, args,
                     "644 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     644, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 644, args, "需要 分母 > 0");
    j50_register_after_action(ctx, owner, 644, /*rounds=*/1, {owner, target, denom},
                              &j50_644_tick);
    return EffectResult::kOk;
}

// ── 579 —— 若当回合未击败对手，则恢复自身最大体力的1/{0} ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_579_not_defeat_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 579, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 579, args,
                     "579 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     579, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 579, args, "需要 分母 > 0");
    j50_register_after_action(ctx, owner, 579, /*rounds=*/1, {owner, target, denom},
                              &j50_579_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(176, effect_176_random_six_pool)
SKILL_EFFECT(644, effect_644_not_defeat_drain)
SKILL_EFFECT(579, effect_579_not_defeat_heal)
