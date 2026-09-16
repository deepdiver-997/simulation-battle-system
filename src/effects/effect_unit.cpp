#include <effects/effect_unit.h>

#include <cstdlib>

#include <fsm/battleContext.h>

namespace {

// 概率 roll：chance_value（字面）优先；否则 chance_arg 指向 EffectArgs.int_args 下标。
// -1 或参数缺失视为必定执行。
bool roll_chance(const EffectArgs& args, int chance_value, int chance_arg) {
    int percent = -1;
    if (chance_value >= 0) {
        percent = chance_value;
    } else if (chance_arg >= 0 && args.int_args && chance_arg < args.int_count) {
        percent = args.int_args[chance_arg];
    }
    if (percent < 0) {
        return true;  // 无概率 → 必定
    }
    if (percent <= 0) {
        return false;
    }
    if (percent >= 100) {
        return true;
    }
    return (std::rand() % 100) < percent;
}

// 解析真实 owner：优先 args.int_args[0]（bind_participants 绑定），缺省回退 unit.actor。
int resolve_actor(const EffectArgs& args, const EffectUnit& unit) {
    if (args.int_args && args.int_count >= 1
        && (args.int_args[0] == 0 || args.int_args[0] == 1)) {
        return args.int_args[0];
    }
    return unit.actor;
}

// 目标解析（相对语义）：0=自身, 1=对手。
int resolve_target(int actor, const EffectUnit& unit) {
    return unit.target == 1 ? 1 - actor : actor;
}

// FIRST/SECOND 行动侧判定（skill 效果经 state_for_owner 镜像到 SECOND 侧，currentState 可靠）。
inline bool is_first_side_state(State s) {
    const int v = static_cast<int>(s);
    return v >= static_cast<int>(State::BATTLE_FIRST_ACTION_START)
        && v <= static_cast<int>(State::BATTLE_FIRST_MOVER_DEATH);
}
inline bool is_second_side_state(State s) {
    const int v = static_cast<int>(s);
    return v >= static_cast<int>(State::BATTLE_SECOND_ACTION_START)
        && v <= static_cast<int>(State::BATTLE_SECOND_MOVER_DEATH);
}

// 前置条件求值（第二刀）：不满足 → 效果不触发。返回 false 走 on_other。
bool condition_holds(BattleContext* ctx, const EffectArgs& args, const EffectUnit& unit) {
    switch (unit.condition) {
        case UnitCondition::None:
            return true;
        case UnitCondition::SameElement: {
            const int actor = resolve_actor(args, unit);
            const int target = resolve_target(actor, unit);
            return ctx->getPet(actor).elementalAttributes[0]
                == ctx->getPet(target).elementalAttributes[0];
        }
        case UnitCondition::FirstMove:
            return is_first_side_state(ctx->currentState);
        case UnitCondition::SecondMove:
            return is_second_side_state(ctx->currentState);
        case UnitCondition::TargetNoAnomaly:
        case UnitCondition::TargetHasAnomaly: {
            const int actor = resolve_actor(args, unit);
            const int target = resolve_target(actor, unit);
            for (int end : ctx->abnormal_status_end_round[target]) {
                if (end != 0) {
                    return unit.condition == UnitCondition::TargetHasAnomaly;  // 有异常
                }
            }
            return unit.condition == UnitCondition::TargetNoAnomaly;  // 无异常
        }
        case UnitCondition::TargetHpBelow: {
            const int actor = resolve_actor(args, unit);
            const int target = resolve_target(actor, unit);
            return ctx->getPet(target).hp < unit.condition_param;
        }
    }
    return true;
}

// 原语细码 → 分支键归一化（首次消费原语返回值）。
BranchKey anomaly_result_to_branch(ApplyAnomalyResult result) {
    switch (result) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
            return BranchKey::Success;
        case ApplyAnomalyResult::TARGET_IMMUNE:
            return BranchKey::Immune;
        case ApplyAnomalyResult::BLOCKED_BY_EFFECT:
            return BranchKey::Blocked;
        case ApplyAnomalyResult::TARGET_DEFEATED:
            return BranchKey::TargetDefeated;
        default:
            return BranchKey::Invalid;
    }
}

BranchKey stat_change_result_to_branch(StatChangeResult result) {
    switch (result) {
        case StatChangeResult::SUCCESS:
            return BranchKey::Success;
        case StatChangeResult::AT_CAP:
            return BranchKey::AtCap;
        default:
            return BranchKey::Invalid;
    }
}

BranchKey heal_result_to_branch(HealResult result) {
    switch (result) {
        case HealResult::SUCCESS:
            return BranchKey::Success;
        default:
            return BranchKey::Invalid;
    }
}

BranchKey fixed_damage_result_to_branch(FixedDamageResult result) {
    switch (result) {
        case FixedDamageResult::SUCCESS:
            return BranchKey::Success;
        case FixedDamageResult::TARGET_DEFEATED:
            return BranchKey::TargetDefeated;
        default:
            return BranchKey::Invalid;
    }
}

// 第二刀新原语细码归一化。
BranchKey pp_reduce_result_to_branch(PpReduceResult result) {
    return result == PpReduceResult::SUCCESS ? BranchKey::Success : BranchKey::Invalid;
}
BranchKey remove_round_effects_result_to_branch(RemoveRoundEffectsResult result) {
    switch (result) {
        case RemoveRoundEffectsResult::SUCCESS: return BranchKey::Success;
        case RemoveRoundEffectsResult::IMMUNE:  return BranchKey::Immune;
        default:                                 return BranchKey::Blocked;
    }
}
BranchKey drain_hp_result_to_branch(DrainHpResult result) {
    switch (result) {
        case DrainHpResult::SUCCESS:          return BranchKey::Success;
        case DrainHpResult::TARGET_DEFEATED:  return BranchKey::TargetDefeated;
        default:                              return BranchKey::Invalid;
    }
}
// 秒杀原语细码 → 分支键归一化（首次消费原语返回值）。
// Kill 标签走 force_hp_to_zero（秒杀族统一入口，2026-09-16 口径）：查秒杀免疫票/瞬杀抑制/
// hp_zero_converted 短路，并 emit EVENT_HP_TO_ZERO——旧的直写 hp=0 版本绕过这一切，
// 会让"咤克斯咒怨转化""奥菲式免疫瞬杀"对解析器路径的秒杀全部失效。
BranchKey hp_zero_result_to_branch(HpZeroResult result) {
    switch (result) {
        case HpZeroResult::EXECUTED:           return BranchKey::Success;
        case HpZeroResult::CONVERTED:          return BranchKey::Blocked;   // 被短路/转化
        case HpZeroResult::TARGET_DOWN:        return BranchKey::TargetDefeated;
        default:                               return BranchKey::Invalid;
    }
}

// 执行主动作（owner 相对语义解析），返回归一化分支键。
BranchKey run_primitive(BattleContext* ctx, const EffectArgs& args, const EffectUnit& unit) {
    const int actor = resolve_actor(args, unit);
    const int target = resolve_target(actor, unit);
    switch (unit.primary_tag) {
        case PrimitiveTag::Anomaly:
            return anomaly_result_to_branch(
                apply_anomaly(ctx, target, unit.param0, unit.param1, actor));
        case PrimitiveTag::StatChange:
            return stat_change_result_to_branch(
                stat_change(ctx, target, unit.param0, unit.param1));
        case PrimitiveTag::Heal:
            return heal_result_to_branch(heal(ctx, target, unit.param0));
        case PrimitiveTag::FixedDamage:
            return fixed_damage_result_to_branch(fixed_damage(ctx, target, unit.param0));
        case PrimitiveTag::PpReduce:
            return pp_reduce_result_to_branch(pp_reduce(ctx, target, unit.param0));
        case PrimitiveTag::RemoveRoundEffects:
            return remove_round_effects_result_to_branch(remove_round_effects(ctx, target));
        case PrimitiveTag::DrainHp:
            return drain_hp_result_to_branch(drain_hp(ctx, actor, target, unit.param0));
        case PrimitiveTag::Kill:
            return hp_zero_result_to_branch(force_hp_to_zero(ctx, target, actor));
        case PrimitiveTag::PowerBoost:
            // 技能威力视图层：直接改 ws（效果在 SKILL_EFFECT 时点跑，ATTACK_DAMAGE 读最终值）。
            ctx->ws.skill_power_view[actor] += unit.param0;
            return BranchKey::Success;
    }
    return BranchKey::Invalid;
}

}  // namespace

BranchKey execute_effect_unit(BattleContext* ctx, const EffectArgs& args, const EffectUnit& unit) {
    if (!ctx) {
        return BranchKey::Invalid;
    }
    // 前置条件（第二刀）：不满足 → 效果不触发（走 on_other/无动作），先于概率 roll。
    if (!condition_holds(ctx, args, unit)) {
        return unit.on_other ? execute_effect_unit(ctx, args, *unit.on_other) : BranchKey::Never;
    }
    // 概率前置：未触发 → on_other 兜底（或返回 Never）
    if (!roll_chance(args, unit.chance_value, unit.chance_arg)) {
        return unit.on_other ? execute_effect_unit(ctx, args, *unit.on_other) : BranchKey::Never;
    }
    // 主动作 → 归一化细码 → 选分支
    const BranchKey key = run_primitive(ctx, args, unit);
    const EffectUnit* branch = nullptr;
    switch (key) {
        case BranchKey::Success:
            branch = unit.on_success;
            break;
        case BranchKey::Immune:
            branch = unit.on_immune;  // Type B：被免疫 → 补偿
            break;
        case BranchKey::Blocked:
            branch = unit.on_blocked;
            break;
        default:
            branch = unit.on_other;
            break;
    }
    if (branch) {
        return execute_effect_unit(ctx, args, *branch);
    }
    return key;
}
