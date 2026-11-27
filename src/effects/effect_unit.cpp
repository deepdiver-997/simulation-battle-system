#include <effects/effect_unit.h>

#include <stdexcept>

#include <fsm/battleContext.h>

ParsedEffectProgram::ParsedEffectProgram(const std::vector<EffectUnit>& units)
    : units_(units) {
    for (std::size_t index = 0; index < units_.size(); ++index) {
        const EffectUnit& unit = units_[index];
        // 严格前向的分支既保证下标有效，也排除了环。
        for (int child : {unit.on_success, unit.on_immune, unit.on_blocked, unit.on_other}) {
            if (child != -1 && (child < 0 || static_cast<std::size_t>(child) <= index
                                || static_cast<std::size_t>(child) >= units_.size())) {
                throw std::invalid_argument("ParsedEffectProgram: invalid branch index");
            }
        }
    }
}

namespace {

// 概率 roll：chance_value（字面）优先；否则 chance_arg 指向 EffectArgs.int_args 下标。
// -1 或参数缺失视为必定执行。
// ⚠️ **只有异常附加这一类概率才过闸门**（亮节族，2026-09-22）：
//    PrimitiveTag::Anomaly 按 ChanceSource::Skill 裁定（解析器生成的都是技能效果）；
//    其余概率（强化/弱化/回血/伤害…）**不走闸门**——亮节原文只管"附加异常效果"，
//    概率挡伤/概率强化等不在它的管辖面内。这就是 ChanceTag 按类切分的落点。
bool roll_chance(BattleContext* ctx, const EffectArgs& args, int chance_value, int chance_arg,
                 int actor, int target, bool is_anomaly, BinaryChancePoint* site) {
    int percent = -1;
    if (chance_value >= 0) {
        percent = chance_value;
    } else if (chance_arg >= 0 && args.int_args && chance_arg < args.int_count) {
        percent = args.int_args[chance_arg];
    }
    if (percent < 0) {
        return true;  // 无概率 → 必定
    }
    if (is_anomaly && target >= 0) {
        percent = ctx->rewrite_anomaly_chance(target, actor, percent,
                                              ChanceSource::Skill);
    }
    if (!site) return ctx->sample_percent(percent);
    site->actor = actor;
    site->target = target;
    return ctx->roll_binary_percent(percent, *site);
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
            // ⚠️ 判活必须用 `roundCount < end_round`，**不能**用 `end != 0`：
            //    `abnormal_status_end_round` 存的是"失效回合"、**过期不回写**（引擎旧习惯），
            //    故 `end != 0` 对早已过期的槽仍判"有异常"（假阳性）。本线加了
            //    `tick_abnormal_statuses` 之后回合扣减点会回写清 0，但**回合中途**（异常在
            //    本回合开始时过期、扣减点还没到）仍会读到旧值 —— 所以判据本身也要对。
            for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
                if (ctx->has_active_abnormal_status(target, id)) {
                    return unit.condition == UnitCondition::TargetHasAnomaly;
                }
            }
            return unit.condition == UnitCondition::TargetNoAnomaly;
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
        case ApplyAnomalyResult::ROLL_FAILED:
            // 概率没过门。⚠️ 正常路径到不了这里——掷骰在 execute_effect_unit_impl 的
            // 概率前置（roll_chance）就已判定，原语入口收到的 chance_pct 恒为"未申报"。
            // 留着是为了让"申报概率 → ROLL_FAILED"这条链在别处复用时语义自洽（= 没发生）。
            return BranchKey::Never;
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

// 执行器主体。skip_condition = 单元准入门放行（"该条已永久无条件"）：跳过**本单元顶层**
// 的条件求值，其余（概率 roll、原语、分支递归）不变；递归进分支子单元时各自按条件判。
BranchKey execute_effect_unit_impl(BattleContext* ctx, const EffectArgs& args,
                                   const ParsedEffectProgram& program, int index,
                                   bool skip_condition, BinaryChancePoint* site = nullptr) {
    const EffectUnit* node = program.unit_at(index);
    if (!ctx || !node) {
        return BranchKey::Invalid;
    }
    const EffectUnit& unit = *node;
    const auto descend = [&](int child, BinaryChanceEdge edge) {
        if (site) site->path.push_back(edge);
        return execute_effect_unit_impl(ctx, args, program, child, /*skip_condition=*/false, site);
    };
    // 前置条件（第二刀）：不满足 → 效果不触发（走 on_other/无动作），先于概率 roll。
    if (!skip_condition && !condition_holds(ctx, args, unit)) {
        return unit.on_other != -1 ? descend(unit.on_other, BinaryChanceEdge::ConditionFailed)
                                   : BranchKey::Never;
    }
    // 概率前置：未触发 → on_other 兜底（或返回 Never）。只有异常附加受闸门改写。
    const bool is_anomaly = unit.primary_tag == PrimitiveTag::Anomaly;
    const int actor = (is_anomaly || site) ? resolve_actor(args, unit) : -1;
    const int target = (is_anomaly || site) ? resolve_target(actor, unit) : -1;
    if (site) site->unit_index = index;
    if (!roll_chance(ctx, args, unit.chance_value, unit.chance_arg, actor, target, is_anomaly, site)) {
        return unit.on_other != -1 ? descend(unit.on_other, BinaryChanceEdge::RollFailed)
                                   : BranchKey::Never;
    }
    // 主动作 → 归一化细码 → 选分支
    const BranchKey key = run_primitive(ctx, args, unit);
    switch (key) {
        case BranchKey::Success:
            return unit.on_success != -1 ? descend(unit.on_success, BinaryChanceEdge::Success) : key;
        case BranchKey::Immune:
            return unit.on_immune != -1 ? descend(unit.on_immune, BinaryChanceEdge::Immune) : key;
        case BranchKey::Blocked:
            return unit.on_blocked != -1 ? descend(unit.on_blocked, BinaryChanceEdge::Blocked) : key;
        default:
            return unit.on_other != -1 ? descend(unit.on_other, BinaryChanceEdge::Other) : key;
    }
}

}  // namespace

BranchKey execute_effect_unit(BattleContext* ctx, const EffectArgs& args,
                              const ParsedEffectProgram& program, int root_index) {
    return execute_effect_unit_impl(ctx, args, program, root_index, /*skip_condition=*/false);
}

BranchKey execute_effect_unit_unconditional(BattleContext* ctx, const EffectArgs& args,
                                            const ParsedEffectProgram& program, int root_index) {
    return execute_effect_unit_impl(ctx, args, program, root_index, /*skip_condition=*/true);
}

void execute_parsed_skill_unit(BattleContext* ctx, const EffectArgs& args, bool skip_root_condition) {
    const auto& ref = args.parsed_unit;
    if (!ctx || args.extra || !ref.program) return;
    if (!ctx->binary_chance_replay) {
        execute_effect_unit_impl(ctx, args, *ref.program, ref.root_index, skip_root_condition);
        return;
    }
    BinaryChancePoint site;
    site.program = ref.program;
    site.arguments = args.owned_int_args;
    site.root_index = ref.root_index;
    site.skip_root_condition = skip_root_condition;
    execute_effect_unit_impl(ctx, args, *ref.program, ref.root_index, skip_root_condition, &site);
}

// ── 单元准入门存储（进程全局）──
// 注册表实例可能是 EffectFactory（moves_lib）也可能是 SoulMarkManager（soul_lib），
// 两处都实现 IEffectRegistry::registerUnitAdmission 并转到这儿，读侧（Skills::register_branch）
// 只认这一份。
namespace {
std::vector<UnitAdmissionFn>& unit_admission_hooks() {
    static std::vector<UnitAdmissionFn> hooks;
    return hooks;
}
}  // namespace

void add_unit_admission(UnitAdmissionFn fn) {
    if (fn) {
        unit_admission_hooks().push_back(fn);
    }
}

bool unit_admission_grants(BattleContext* ctx, int owner, int skill_id, int unit_index,
                           int condition) {
    for (UnitAdmissionFn fn : unit_admission_hooks()) {
        if (fn && fn(ctx, owner, skill_id, unit_index, condition)) {
            return true;   // 任一钩子放行即放行（门是全局的，条数按 dylib 计）
        }
    }
    return false;
}
