// J94 · 击败PP/护盾固伤族 —— 1286×14 / 1272×14（2026-10-03，worktree ../agents/j94）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1286（args_num=0）"当回合击败对手则恢复自身所有技能PP值"：
//   - J12 桥击败门（AFTER_ACTION 双侧探针 + acting 过滤 + 对手在场宠 hp≤0）
//     → g_core->pp_restore(self, amount=999, only_empty=false)（原语逐槽 clamp 到
//     maxPP；2076 吸取窗同原语通道；"所有技能"= 全槽回满）；
//   - 零参 → bind_participants 仍带 [owner,target]（964 同款）。
//   - 实例：枪托打击 34419 [1286]（物140 纯链）。
// 1272（args_num=0）"附加自身当前护盾值等量的固定伤害"：
//   - J43 1017 直系：shield_bank_.total() 读总盾 → consume 全消耗（EVENT_SHIELD_BROKEN
//     逐盾补发）→ fixed_damage(对手, consumed)；
//   - 零参 bind 补位；无盾（total≤0）不打 0 伤（1017 同守卫）。
//   - 实例：法则崩毁 35827 [1272]（物120 纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1286/1272 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1286=0/1272=0 与 effect_info.args_num 一致；28 载体切片自洽（全零参）。
// 先例：J12（击败门探针框架）、J43 1017（shield_bank total/consume + fixed）、
//   J58（pp 原语通道）、pp_restore（CoreApi 133）。
// 场景回归：test/scenario/scenario_1830_defeat_pp_shield.cpp（1830 号段）。

#include "common.h"

#include <memory>

namespace {

// AFTER_ACTION 桶的当前行动方（J12 同款小工具）。
int j94_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// 击败门判定（J12 同构，判据取补：对手在场宠已倒）。
bool j94_gate_pass(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return false;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1 || j94_bucket_acting_side(c) != me) {
        return false;
    }
    return c->getPet(1 - me).hp <= 0;
}

// 1286 门探针：击败 → PP 全回。
EffectResult j94_1286_probe(BattleContext* c, const EffectArgs& a) {
    if (!j94_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (!g_core || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    (void)g_core->pp_restore(c, self, /*amount=*/999, /*only_empty=*/false);
    return EffectResult::kOk;
}

// 门挂载（J12 同构：双侧探针、duration=1）。
void j94_arm_gate(BattleContext* ctx, int effect_id, int self, std::vector<int> payload,
                  EffectFn probe) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, self, -1, EffectArgs(payload), probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
}

}  // namespace

// ── 1286 —— 当回合击败对手则恢复自身所有技能PP值 ──
// int_args = [owner, target]（args_num=0，bind 补位）。
EffectResult effect_1286_defeat_restore_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1286, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1286, args,
                     "1286 布局=[owner,target]（零参补位）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1286, args, "owner 越界");
    j94_arm_gate(ctx, 1286, owner,
                 std::vector<int>(args.int_args, args.int_args + args.int_count),
                 &j94_1286_probe);
    return EffectResult::kOk;
}

// ── 1272 —— 附加自身当前护盾值等量的固定伤害 ──
// int_args = [owner, target]（args_num=0，bind 补位）。
EffectResult effect_1272_shield_value_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1272, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1272, args,
                     "1272 布局=[owner,target]（零参补位）");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1 && self != target,
                     1272, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int total = ctx->getPet(self).shield_bank_.total();
    if (total <= 0) {
        return EffectResult::kOk;   // 无盾：消耗 0 → 不打 0 伤（1017 同守卫）
    }
    int broken = 0;
    const int consumed = ctx->getPet(self).shield_bank_.consume(total, &broken);
    for (int i = 0; i < broken; ++i) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, -1, self});
    }
    if (consumed > 0) {
        (void)g_core->fixed_damage(ctx, target, consumed);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1286, effect_1286_defeat_restore_pp)
SKILL_EFFECT(1272, effect_1272_shield_value_fixed)
