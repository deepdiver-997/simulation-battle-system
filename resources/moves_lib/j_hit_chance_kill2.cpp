// J 组 J130 · 命中概率秒杀族 —— 691×13（2026-10-04，worktree ../agents/j130）
//
// 家族语义（effect_info 官方模板即权威）：
//   691（arg_count=1）"命中后{0}%秒杀对手"——{0}∈{5,6,8,10,15,100}
//
// 实现口径（J23 36 直系参数泛化，逐行同构）：
//   - 495 AFTER_ACTION 延迟器（伤害落地后判秒杀，避免"先秒后伤"乱序）——SKILL_EFFECT
//     注册 AFTER_ACTION 双桶一次性 probe（duration=1、start=当回合）；
//   - 命中门 = ws.skill_exec_result[self]==HIT（miss/无效不触发；EVENT_HIT 投递先于
//     注册、watcher 收不到——36 同款不用事件面）；
//   - roll_percent({0}) → g_core->force_hp_to_zero（秒杀族唯一入口：免疫票/短路转化/
//     EVENT_HP_TO_ZERO 在原语内统一处理）+ 存活门（已被本次攻击打死则不重复，2295）。
//
// 载体切片（unity 库 json_each 精确匹配 13/13；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-04）：691 [概率] ×13 单参。
//   纯链×6：雷光振杀 19507 [10] 特130 / 伏尸狂念 31927 [8] / 铭锤浩瀚 33178 [5] /
//   未来·绝命火焰 33427 [10] / 孤魂消散 35172 [10] / 缔灵之触 36311 [15] 物140
//   100% 载体：五衰 13744 [100]（谱尼链全实现）/ 跪地求饶 28186 [100] 属性技
//   （链 72 未实现哑=miss 自罚，必中载体下永不触发、对链激活免疫——确定性场景载体）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 691 无
// 硬编码 case ②effect_meta/battle_effects 无行 ③moves_lib 并集无注册——真缺口。
// 先例：j_status_amp（J23：36 同模板 probe 全套）、force_hp_to_zero（CoreApi 151）、
//   roll_percent（common.h）。
// 场景回归：test/scenario/scenario_2240_hit_chance_kill2.cpp（2240 号段）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（J23/J109 同款小工具）。
int j130_bucket_acting_side(const BattleContext* ctx) {
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

// 691 probe：命中且对手存活 → roll → 秒杀（36 同款）。
EffectResult j130_691_kill_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->force_hp_to_zero || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int pct = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (j130_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中后"门：miss 不触发
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死（存活门，2295）
    }
    if (roll_percent(pct)) {
        (void)g_core->force_hp_to_zero(c, opp, self);
    }
    return EffectResult::kOk;
}

// ── 691 —— 命中后{0}%秒杀对手 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_691_hit_chance_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 691, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 691, args,
                     "691 布局=[owner,target,概率]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 691, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0, 691, args, "需要 概率 > 0");
    const std::vector<int> payload{self, 1 - self, pct};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(691, 0, self, -1, EffectArgs(payload), &j130_691_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 691;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(691, effect_691_hit_chance_kill)
