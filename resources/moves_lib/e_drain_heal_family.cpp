// E10 · 吸取/回复转化族批产（2026-09-29，工单：docs_local/docs/05-任务清单/E组批量效果工单.md E10）
//
// 家族成员（effect_info 模板 × args_num）：
//   703（2 参 [rounds, denom]）"{0}回合内若对手使用属性技能降低对手最大体力的1/{1}"
//       —— 破魂引玉 17093 [2,3]、裂天蔽日 23813 [1,1]、灭世万法 23905 [3,4] 等 40 技。
//   417（2 参 [rounds, pct]）  "{0}回合内自身攻击技能造成伤害的{1}%会恢复自身体力"
//       —— 幻灭击 15436 [3,30]、残影追命啸 16648 [1,40]、爆魔花仙炮 17568 [2,50] 等 38 技。
//   1449（2 参 [rounds, denom]）"{0}回合内对手无法通过自身技能恢复体力且受到原本恢复量
//       1/{1}的百分比伤害" —— 石鬼守护 27437 [3,3]、寒冬润养 26996 [3,3]、海水倒流 27124
//       [3,3] 等 39 技。
//
// 动工三查（2026-09-29）：内核无 703/417/1449 硬编码；moves_lib 无既有 SKILL_EFFECT ——
// 真缺口。
//
// 落点：
// ★ 703 = 663 窗口 tick 同款（FIRST/SECOND_AFTER_ACTION 双桶 InRounds，tick 按 acting
//   side 门控）：对手 acting && 本回合真点了技能（used_skill 门，防被控空转误触）&&
//   选的不是攻击技（selected_skill_is_attack 取反）→ 对手掉 最大体力/denom（PERCENT_VALUE
//   百分比伤，1709 同款换算源）。"降低对手最大体力的1/N"= 造成等量伤害（天启 1298"扣除
//   最大体力的1/4"同语族），非上限削减（883 的上限语义带"上限"字样，记档待实测翻案）。
// ★ 417 = 1249 吸血同通道（读 ws.raw_attack_damage 裸伤台账 ×pct% → heal_amount），
//   套 InRounds 窗口 tick：owner acting && 真点技 && 是攻击技 && raw>0 → 回血。
//   heal_amount 天然吃封回血/恢复修正门（"会恢复自身体力"是回复不是直写）。
// ★ 1449 前半 = 679 同款：HEAL_BLOCK 票挂对手（coverage_all + InRounds 时长）。
//   ⚠️ 后半跳过（记档待内核线）："受到原本恢复量1/N的百分比伤害"需要被封恢复量——
//   heal_impl 在 HEAL_BLOCK 分支把 last_heal_amount 清 0 且无事件（battle_primitives.cpp
//   封回血分支），插件侧不可观测；rule_center 亦无恢复转化票（仅药剂反噬预留位，管不到
//   技能恢复）。需内核暴露"被封恢复量"（如 last_heal_amount 记原值 + 封锁事件）后补。
#include "common.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace {

// ── AFTER_ACTION 桶的当前行动方（d_fifth_skills 同款）────────────────────────
int bucket_acting_side(const BattleContext* ctx) {
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

// 窗口挂 AFTER_ACTION 双桶。⚠️ 桶按 (时点, 行动方) 执行——execute_registered_actions
// 只跑"行动方名下"的条目（handle_BattleFirst/SecondAfterAction 实证：首/次行动方的 id
// 决定执行谁名下的桶），所以要在**对手**行动后触发就得把条目挂对手名下（tick 内再按
// acting==opp 门控）。起算用手工 registered_round=施放回合：round_effect_start_round 的
// InRounds 后出手顺延是对"自己结算错过"的启发式，对"对手行动"型触发不适用（施放后
// 对手的首次行动就应纳入窗口；若对手本回合先于施放行动，条目尚未注册、天然不计）。
// ★ 同构先例 663（burn_trio_skills.cpp）按施放者名下挂双桶——对手行动时条目不执行，
//   属潜伏死代码（他族文件禁改；已在上报中记档，交协调者处理）。
void register_after_action_window_on(BattleContext* ctx, int bucket_owner, int effect_id,
                                     int rounds, const std::vector<int>& ints, EffectFn fn) {
    if (!ctx || bucket_owner < 0 || bucket_owner > 1 || rounds <= 0 || !fn) {
        return;
    }
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, bucket_owner, rounds, EffectArgs(ints), fn);
        auto ce = std::make_unique<ContinuousEffect>(e, st, bucket_owner, rounds,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, bucket_owner, std::move(ce));
    }
}

// ── 703 —— {0}回合内若对手使用属性技能，降低对手最大体力的1/{1} ────────────────
EffectResult effect_703_opp_status_punish_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (bucket_acting_side(ctx) != opp) {
        return EffectResult::kOk;   // 只在对手行动后结算
    }
    if (!executed_skill_this_round(ctx, opp) || executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 真点了属性技能才触发（被控/嗑药不算，攻击技不算）
    }
    if (ctx->getPet(opp).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, max_hp_of(ctx, opp) / denom,
                                       DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_703_opp_status_punish(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 703, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 703, args, "需要 [owner,target,rounds,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 703, args,
                     "owner/target 越界");
    if (rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_window_on(ctx, opp, 703, rounds, {owner, opp, denom},
                                    &effect_703_opp_status_punish_tick);
    return EffectResult::kOk;
}

// ── 417 —— {0}回合内自身攻击技能造成伤害的{1}%会恢复自身体力 ──────────────────
EffectResult effect_417_lifesteal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 攻击技能造成伤害才触发
    }
    const int raw = ctx->ws.raw_attack_damage[owner];
    if (raw <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, raw * pct / 100);
    return EffectResult::kOk;
}

EffectResult effect_417_lifesteal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 417, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 417, args, "需要 [owner,target,rounds,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 417, args,
                     "owner/target 越界");
    if (rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_window_on(ctx, owner, 417, rounds, {owner, opp, pct},
                                    &effect_417_lifesteal_tick);
    return EffectResult::kOk;
}

// ── 1449 —— {0}回合内对手无法通过自身技能恢复体力（679 同款；转化子句跳过见文件头）─
EffectResult effect_1449_heal_block(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1449, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 1449, args, "需要 [owner,target,rounds,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1449, args,
                     "owner/target 越界");
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1449,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1449);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(703, effect_703_opp_status_punish);
SKILL_EFFECT(417, effect_417_lifesteal_window);
SKILL_EFFECT(1449, effect_1449_heal_block);
