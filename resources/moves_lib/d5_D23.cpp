// D23 · 希拓 4622 第五技「神煌炎舞斩 37222」缺失效果 1002（夜跑 night-2，2026-09-28）
//
// 37222 链 = [1002, 942, 1960]、args [1, 3, 3]：1002=[1]（本文件）、942=[3]（已接）、
// 1960=[3]（已接——STAT_CLEAR 族首用例）。
// 4399 图鉴（神煌炎舞斩）："必中；若对手处于异常状态则先制+1；消除对手回合类效果，
// 消除成功则吸取对手最大体力的1/3；击败对手则令自身3回合内的能力提升状态无法被消除
// 或吸取"。
//
// 1002 "若对手处于异常状态则先制+1"：
//   - 条件先制族（610 遇天敌先制 / 1782 有盾先制同构）：MOVE_RIGHT 常驻节点
//     （guard 一次性注册，999 回合）——所选技==37222 且对手 has_any_active_abnormal_status
//     → preemptive_level += 1；
//   - ⚠️ 结构性偏差记档：先手权结算先于 SKILL_EFFECT → 每场战斗第 1 次使用不加先制
//     （官方应加），第 2 次起正常（与 1098/1782 同根源，待拍板）。
//
// 场景回归：test/scenario/scenario_390_xituo_4622_1002.cpp。

#include "common.h"

namespace {

constexpr int kSkillId = 37222;              // 神煌炎舞斩
constexpr int k1002ArmedKeyBase = 990100210; // 常驻节点已注册旗标（owner 侧）

// 本回合 owner 所选技能是否为神煌炎舞斩（MOVE_RIGHT 时 roundChoice 已写入）。
bool selected_is_37222(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    return skills[static_cast<std::size_t>(slot)].id == kSkillId;
}

// ── 1002 的 MOVE_RIGHT 节点：对手处于异常 → 先制 +1 ──
EffectResult effect_1002_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !selected_is_37222(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(1 - owner)) {
        return EffectResult::kOk;   // 对手无异常：不加
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

// ── 1002 —— 若对手处于异常状态则先制+1（武装节点）──
EffectResult effect_1002_conditional_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = k1002ArmedKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 常驻节点只注册一次
    }
    ctx->plugin_storage[key] = 1;
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1002,
                          /*rounds=*/999, {owner}, &effect_1002_priority_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1002, effect_1002_conditional_priority);
