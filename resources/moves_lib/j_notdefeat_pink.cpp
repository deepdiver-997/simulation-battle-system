// J 组 J58 · 未击败/回合末粉伤族 —— 3 宏/54 载体（2026-10-03，worktree ../agents/j58）
//
// 家族语义（effect_info 模板即权威）：
//   752  [1] 若对手处于异常状态则回合结束时减少对手1/{0}最大体力
//   1285 [1] 当回合未击败对手则减少对方所有技能PP{0}点
//   411  [3] 附加对手当前体力值{0}%的百分比伤害，连续使用每次增加{1}%，最高{2}%
//
// 实现口径：
//   - 752 = 异常门（F3 1214 遍历 0..kOfficialAbnormalStatusMaxId 逐
//     has_active_anomaly，J52 768 同款"种数>0"判据）+ AFTER_ACTION 单发
//     deal_pink_damage(FIXED, max_hp/{0})——J50 644 的 max 形（"回合结束时"按 J50
//     家族口径落 AFTER_ACTION 单发，双侧注册、桶只跑行动方）。max_hp 读
//     numericalBase[HP]（battleContext 同款）。
//   - 1285 = 未击败门（J50 644/579 同门：行动末对手 hp>0）+ pp_reduce 全槽
//     {0}（原语，钳 ≥0、逐槽 EVENT_PP_REDUCED；common.h pp_zero_all 999 归零
//     同通道）。
//   - 411 = deal_pink_damage(FIXED, 当前体力×pct/100)（J50 644 当前形）+
//     uses 递增：pct = min(base+(n-1)*step, cap)（J52 1328 同款 owner×技能 分账、
//     首用=base、审计槽无随机断言；"连续使用"按 1328/782 已定口径=本技能每用
//     累计、换技不断账不重置）。当前体力 = SKILL_EFFECT 时点实扣前快照。
//
// plugin_storage 键段（990158x，grep 全库无撞）：
//   990158100 + owner*40000 + skill_id%40000   411 已用次数
//   990158200 + owner                          411 本次申报 pct（审计槽）
//
// 载体切片（unity 库 json_each 核对；场景载体全走"兄弟已实现且良性"链或纯链，
// 防链激活翻车——速查 §五）：
//   752  纯链：36922 念力封锁 [3]、36961 冻感舞蹈 [4]
//   1285：33608 寂·凛风残念 [1224,1129,1285]|[2,30,2,1]（1224=消回合类+自必暴、
//         1129=30%×2 倍伤——只动红伤，PP 断言免疫）
//   411 ：34167 冷瞳裁决 [48,411]|[3,20,10,40]（48=自身免疫异常，无伤害无 PP）
//
// 动工前置三查（2026-10-02 立项复核 + 2026-10-03 动工复核）：①内核 grep 752/1285/411
// 无硬编码 ②custom_effect_overrides 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1440_notdefeat_pink.cpp（号段 1440-1449）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k411UsesKeyBase = 990158100;   // +owner*40000 + skill_id%40000：已用次数
constexpr int k411PctAuditBase = 990158200;  // +owner：本次申报 pct（审计槽）

// 对手有任一活跃异常（"处于异常状态"门；J52 768 同款遍历口径）。
bool j58_foe_statused(BattleContext* ctx, int target) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            return true;
        }
    }
    return false;
}

// 未击败门：行动末对手仍存活（J50 934 存活镜像）。
bool j58_not_defeated(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return false;
    }
    const int opp = args.int_args[1];
    if (opp < 0 || opp > 1) {
        return false;
    }
    return ctx->getPet(opp).hp > 0;
}

// AFTER_ACTION 双侧单发注册（InRounds rounds=1 含当回合；桶只跑行动方，52 教训）。
void j58_register_after_action(BattleContext* ctx, int owner, int effect_id,
                               std::vector<int> payload, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, effect_id, /*rounds=*/1, payload, fn);
    }
}

// ── 752 的 AFTER_ACTION tick —— payload={owner, target, denom}；对手有异常 → 削 max 1/{0}。
EffectResult j58_752_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (denom <= 0 || !j58_foe_statused(ctx, target)) {
        return EffectResult::kOk;   // 门：对手处于异常状态
    }
    const int max_hp = ctx->getPet(target).numericalBase[NumericalPropertyIndex::HP];
    const int amount = max_hp / denom;
    if (amount > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 1285 的 AFTER_ACTION tick —— payload={owner, target, reduce}；未击败 → 全槽 PP-{0}。
EffectResult j58_1285_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!j58_not_defeated(ctx, args)) {
        return EffectResult::kOk;   // 门：当回合未击败对手
    }
    const int target = args.int_args[1];
    const int reduce = args.int_args[2];
    if (reduce > 0 && g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, reduce);
    }
    return EffectResult::kOk;
}

// ── 752 —— 若对手处于异常状态则回合结束时减少对手1/{0}最大体力 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_752_statused_max_fraction(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 752, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 752, args,
                     "752 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     752, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 752, args, "需要 分母 > 0");
    j58_register_after_action(ctx, owner, 752, {owner, target, denom}, &j58_752_tick);
    return EffectResult::kOk;
}

// ── 1285 —— 当回合未击败对手则减少对方所有技能PP{0}点 ──
// int_args = [owner, target, reduce]（args_num=1）。
EffectResult effect_1285_not_defeat_pp_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1285, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1285, args,
                     "1285 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int reduce = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1285, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(reduce > 0, 1285, args, "需要 点数 > 0");
    j58_register_after_action(ctx, owner, 1285, {owner, target, reduce}, &j58_1285_tick);
    return EffectResult::kOk;
}

// ── 411 —— 附加对手当前体力{0}%的百分比伤害，连用每次+{1}%，最高{2}% ──
// int_args = [owner, target, base, step, cap]（args_num=3）。
EffectResult effect_411_ramp_cur_pct_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 411, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 411, args,
                     "411 布局=[owner,target,base,step,cap]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || base <= 0 || step <= 0
        || cap <= 0) {
        return EffectResult::kOk;
    }
    // uses 计数（owner×技能 分账，1328 同款；取不到技能槽退化为 skill_id=0 账本）
    const int slot = ctx->executing_skill_slot(owner);
    const int skill_id = (slot >= 0 && slot < 5)
                             ? ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id
                             : 0;
    const int key = k411UsesKeyBase + owner * 40000 + (skill_id % 40000);
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    const int pct = std::min(base + (uses - 1) * step, cap);   // 首用 = base
    ctx->plugin_storage[k411PctAuditBase + owner] = pct;
    const int cur = ctx->getPet(target).hp;   // 实扣前快照（SKILL_EFFECT 在伤害管线前）
    const int amount = cur * pct / 100;
    if (amount > 0 && cur > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(752, effect_752_statused_max_fraction)
SKILL_EFFECT(1285, effect_1285_not_defeat_pp_reduce)
SKILL_EFFECT(411, effect_411_ramp_cur_pct_pink)
