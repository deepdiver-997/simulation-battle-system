#include <fsm/battle_clone.h>

#include <db/official_data_repository.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/iControlBlock.h>

#include <any>
#include <array>
#include <cstring>
#include <iterator>
#include <map>
#include <string_view>
#include <typeinfo>
#include <utility>

namespace {
class BranchControl final : public IControlBlock, public BattleCloneControl {
public:
    BranchControl(const BattleFsm& fsm, const std::unique_ptr<BattleContext>& context)
        : fsm_(fsm), context_(context) {}

    bool clone_source_certified(const BattleContext& ctx, const BattleFsm& fsm) const noexcept override {
        return context_.get() == &ctx && &fsm_ == &fsm;
    }
    void wait_for_input(BattleContext*) override {}
    void async_write(int, const std::string& message, BattleContext*, BattleFsm*) override {
        if (error.empty()) error = message;
    }
    void on_fsm_paused(BattleContext*) override {}
    void store_step_payload(std::unique_ptr<StepPayload> payload) noexcept override {
        step_payload_ = std::move(payload);
    }
    std::unique_ptr<StepPayload> take_step_payload() noexcept override {
        return std::move(step_payload_);
    }

    std::string error;
private:
    const BattleFsm& fsm_;
    const std::unique_ptr<BattleContext>& context_;
    std::unique_ptr<StepPayload> step_payload_;
};

bool valid_roster_slots(const BattleContext& ctx) {
    return ctx.on_stage[0] >= 0 && ctx.on_stage[0] < 6 &&
           ctx.on_stage[1] >= 0 && ctx.on_stage[1] < 6;
}

bool owns_args(const EffectArgs& args) {
    // EffectArgs copies its vectors and rebinds the views, but does not own extra.
    const auto owns = [](const auto* view, int count, const auto& backing) {
        return count >= 0 && static_cast<std::size_t>(count) == backing.size() &&
               view == (backing.empty() ? nullptr : backing.data());
    };
    return !args.extra && owns(args.int_args, args.int_count, args.owned_int_args) &&
           owns(args.vec_offsets, args.vec_count, args.owned_vec_offsets) &&
           owns(args.vec_sizes, args.vec_count, args.owned_vec_sizes);
}

bool has_no_parsed_unit(const EffectArgs& args) {
    return !args.parsed_unit.program && args.parsed_unit.root_index == -1;
}

bool cloneable_priority_effect(const Effect& effect) {
    return Skills::is_base_priority_effect(effect) && owns_args(effect.args) &&
           has_no_parsed_unit(effect.args);
}

bool cloneable_skill_effect(const Effect& effect) {
    if (!owns_args(effect.args)) return false;
    if (Skills::is_parsed_unit_effect(effect)) return true;
    return has_no_parsed_unit(effect.args) && EffectFactory::is_clone_audited(effect);
}

bool check_storage(const std::map<int, std::any>& storage, std::string_view name,
                   std::string& error) {
    for (const auto& [key, value] : storage) {
        const auto& type = value.type();
        if (!value.has_value() || type == typeid(int) || type == typeid(bool) ||
            type == typeid(std::string) || type == typeid(std::pair<int, int>) ||
            type == typeid(std::array<int, 12>)) continue;
        error = std::string(name) + "[" + std::to_string(key) + "]: unverified payload " + type.name();
        return false;
    }
    return true;
}

bool check_roster(const BattleContext& ctx, std::string& error) {
    for (int side = 0; side < 2; ++side) {
        const auto& roster = ctx.seerRobot[side].elfPets;
        for (std::size_t slot = 0; slot < roster.size(); ++slot) {
            const auto& pet = roster[slot];
            const auto path = [&] { return "pet " + std::to_string(side) + ":" + std::to_string(slot) + " "; };
            if (!check_storage(pet.soulmark_storage, "soulmark_storage", error) ||
                !check_storage(pet.on_stage_storage, "on_stage_storage", error)) {
                error = path() + error;
                return false;
            }
            const auto& soul = pet.soulMark;
            if (!owns_args(soul.args) || !has_no_parsed_unit(soul.args) || soul.effect ||
                !soul.program_.empty() || soul.has_program_ || soul.hooks_.on_enter || soul.hooks_.on_exit) {
                error = path() + "has unverified soul mark or arguments";
                return false;
            }
            for (const auto& skill : pet.skills) {
                if (!skill.carryEffects.empty() || !skill.arg_overrides_.empty() || !skill.disabled_.empty()) {
                    error = path() + "skill " + std::to_string(skill.id) + " has unverified effects or load state";
                    return false;
                }
                for (const auto& branch : skill.effectBranches) {
                    for (const auto& node : branch.second) {
                        if (!cloneable_skill_effect(node.effect)) {
                            error = path() + "skill " + std::to_string(skill.id) + " has an unverified branch effect";
                            return false;
                        }
                    }
                }
                for (const auto& node : skill.selection_effects_) {
                    if (node.registerState != State::BATTLE_FIRST_MOVE_RIGHT ||
                        !cloneable_priority_effect(node.effect)) {
                        error = path() + "skill " + std::to_string(skill.id) + " has unverified selection effects";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

template<class T> void assign(T& dst, const T& src) { dst = src; }
template<class T, std::size_t N> void assign(T (&dst)[N], const T (&src)[N]) {
    for (std::size_t i = 0; i < N; ++i) assign(dst[i], src[i]);
}
}

struct BattleCloneBranch::Impl {
    BattleFsm fsm{false};
    std::unique_ptr<BattleContext> battle;
    BranchControl control{fsm, battle};

    ~Impl() { battle.reset(); }  // Keep control alive through context destruction.
};

BattleCloneBranch::BattleCloneBranch(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
BattleCloneBranch::~BattleCloneBranch() = default;

std::unique_ptr<BattleCloneBranch> BattleCloneBranch::start_local(
    const SeerRobot* robots, unsigned int seed, int max_rounds,
    bool effect_arg_fail_fast, std::string& error) {
    auto impl = std::make_unique<Impl>();
    impl->battle = std::make_unique<BattleContext>(&impl->control, robots, false, seed);
    if (impl->control.error.empty()) {
        impl->battle->max_rounds = max_rounds;
        impl->battle->effect_arg_fail_fast = effect_arg_fail_fast;
        impl->fsm.run(impl->battle.get());
    }
    error = std::move(impl->control.error);
    if (!error.empty()) return nullptr;
    return std::unique_ptr<BattleCloneBranch>(new BattleCloneBranch(std::move(impl)));
}

BattleContext& BattleCloneBranch::context() noexcept { return *impl_->battle; }
const BattleContext& BattleCloneBranch::context() const noexcept { return *impl_->battle; }
const BattleFsm& BattleCloneBranch::fsm() const noexcept { return impl_->fsm; }

BattleCloneResult BattleCloneBranch::run() {
    auto& ctx = context();
    if (!valid_roster_slots(ctx)) return {false, "invalid active pet slot"};
    if (!ctx.is_empty) {
        std::string error;
        if (ctx.finish_flag || (ctx.currentState != State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
                                ctx.currentState != State::CHOOSE_AFTER_DEATH)) {
            error = "input supplied outside a decision point";
        } else if (ctx.m_buffer.size() != 4 * sizeof(int)) {
            error = "invalid input length";
        } else {
            int player;
            std::memcpy(&player, ctx.m_buffer.data(), sizeof(player));
            if (player < 0 || player > 1) error = "invalid player id";
            else if (ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
                     ctx.operation_collected[player]) error = "action already submitted";
        }
        if (!error.empty()) {
            ctx.m_buffer.clear();
            ctx.is_empty = true;
            return {false, std::move(error)};
        }
    }
    impl_->control.error.clear();
    impl_->fsm.run(&ctx);
    const auto& error = impl_->control.error;
    return {error.empty(), error};
}

BattleCloneResult BattleCloneBranch::submit(int player, BattleCloneAction action, int index) {
    auto& ctx = context();
    if (player < 0 || player > 1) return {false, "invalid player id"};
    if (!valid_roster_slots(ctx) || ctx.finish_flag || !ctx.need_input())
        return {false, "branch is not waiting for input"};
    if (ctx.currentState == State::CHOOSE_AFTER_DEATH && action != BattleCloneAction::ChoosePet)
        return {false, "death replacement requires a pet"};
    if (ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT && ctx.operation_collected[player])
        return {false, "action already submitted"};

    using Action = BattleFsm::ActionType;
    Action engine_action;
    switch (action) {
        case BattleCloneAction::ChoosePet:
            if (index < 0 || index >= 6) return {false, "invalid pet index"};
            engine_action = Action::CHOOSE_PET;
            break;
        case BattleCloneAction::SelectSkill:
            if (index < 0 || index >= 5) return {false, "invalid skill index"};
            engine_action = Action::SELECT_SKILL;
            break;
        case BattleCloneAction::UseMedicine: {
            const auto& stock = ctx.seerRobot[player].medicines;
            if (index < 0 || static_cast<std::size_t>(index) >= stock.size())
                return {false, "invalid medicine index"};
            const auto item = std::next(stock.begin(), index);
            const int slot = ctx.on_stage[player];
            if (item->second <= 0 || ctx.getPet(player).hp <= 0 || ctx.is_death_registered(player, slot) ||
                !official_data::OfficialDataStore::instance().repository().load_battle_item(item->first))
                return {false, "medicine is not usable"};
            engine_action = Action::USE_MEDICINE;
            break;
        }
        case BattleCloneAction::Skip:
            engine_action = Action::NONE;
            break;
        default: return {false, "invalid action"};
    }
    const int packet[] = {player, static_cast<int>(engine_action), index, 0};
    const char* bytes = reinterpret_cast<const char*>(packet);
    ctx.m_buffer.assign(bytes, bytes + sizeof(packet));
    ctx.is_empty = false;
    return run();
}

std::unique_ptr<BattleCloneBranch> BattleClone::owned_at_decision(
    const BattleContext& source, std::string& error) {
    const auto fail = [&](const char* message) -> std::unique_ptr<BattleCloneBranch> {
        error = message;
        return nullptr;
    };
    if (source.binary_chance_replay) return fail("source has an active binary chance replay");
    const auto* controller = dynamic_cast<const BattleCloneControl*>(source.control_block_);
    const auto* fsm = source.m_fsm;
    if (!fsm || !controller || !controller->clone_source_certified(source, *fsm))
        return fail("source controller cannot certify a paused battle");
    if (!valid_roster_slots(source) || source.finish_flag || source.debug_step_mode ||
        !source.breakpoints.empty() || !source.need_input() ||
        (source.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
         source.operation_collected[0] && source.operation_collected[1]))
        return fail("source is not at a stable input decision");
    if (source.on_finish || !source.death_interceptors.empty() || !source.invalid_skill_damage_hooks.empty() ||
        source.ws.combo_count_resolver[0] || source.ws.combo_count_resolver[1] ||
        source.ws.combo_hit_hook[0] || source.ws.combo_hit_hook[1])
        return fail("source contains an unverified battle callback");
    if (!check_storage(source.plugin_storage, "plugin_storage", error) || !check_roster(source, error))
        return nullptr;

    const auto& events = source.event_center_;
    if (events.draining_ || !events.pending_.empty())
        return fail("source contains pending or in-flight events");
    for (const auto& [id, watcher] : events.watchers_) {
        if (!watcher.clone_copyable) {
            error = "source contains an unverified watcher (id " + std::to_string(id) + ")";
            return nullptr;
        }
    }
    if (events.sink_ != source.tape_.event_sink())
        return fail("source has an external event sink");
    const auto& rules = source.rule_center_;
    for (const auto& ticket : rules.all_) {
        if (ticket.condition || ticket.pool_mod || ticket.anomaly_condition || ticket.anomaly_roll)
            return fail("source contains an unverified rule callback");
    }

    auto branch = std::make_unique<BattleCloneBranch::Impl>();
    if (fsm->stateHandlerMap != branch->fsm.stateHandlerMap)
        return fail("source FSM uses a non-default handler table");
    branch->fsm.id_debug = fsm->id_debug;
    branch->fsm.verbose_trace_ = fsm->verbose_trace_;
    branch->battle.reset(new BattleContext(&branch->control, source.seerRobot, source.rng()));
    auto& dst = *branch->battle;
    dst.m_fsm = &branch->fsm;

    const auto copy_bucket = [&](const TimedBucket& in, TimedBucket& out, bool skill_bucket) {
        for (const auto& [state, sides] : in.buckets_) {
            auto& buckets = out.buckets_[state];
            for (int side = 0; side < 2; ++side) {
                for (const auto& [key, effect] : sides[side]) {
                    if (!skill_bucket || !effect || typeid(*effect) != typeid(ContinuousEffect)) return false;
                    const Effect& data = *effect->getEffect();
                    if (!cloneable_skill_effect(data) &&
                        !(state == State::BATTLE_FIRST_MOVE_RIGHT && cloneable_priority_effect(data))) return false;
                    buckets[side].emplace(key, std::make_unique<ContinuousEffect>(*effect));
                }
            }
        }
        out.next_anonymous_key_ = in.next_anonymous_key_;
        assign(out.active_round_count_, in.active_round_count_);
        return true;
    };
    if (!copy_bucket(source.skills_effects, dst.skills_effects, true) ||
        !copy_bucket(source.soul_mark_effects, dst.soul_mark_effects, false) ||
        !copy_bucket(source.suit_effects, dst.suit_effects, false) ||
        !copy_bucket(source.passive_effects, dst.passive_effects, false) ||
        !copy_bucket(source.updater_effects, dst.updater_effects, false))
        return fail("source contains an unverified timed effect or effect arguments");
    if (!source.damage_pipeline_.clone_into(dst.damage_pipeline_))
        return fail("source contains an unverified red-damage callback");
    if (!source.pink_damage_pipeline_.clone_into(dst.pink_damage_pipeline_))
        return fail("source contains an unverified pink-damage callback");

    dst.rule_center_.all_ = rules.all_;
    dst.rule_center_.next_source_id_ = rules.next_source_id_;
    assign(dst.rule_center_.round_count_, rules.round_count_);
    dst.tape_ = source.tape_;
    dst.event_center_.sink_ = dst.tape_.event_sink();
    dst.event_center_.watchers_ = events.watchers_;
    dst.event_center_.by_type_ = events.by_type_;
    dst.event_center_.next_id_ = events.next_id_;
    assign(dst.event_center_.defeat_effects_suppressed, events.defeat_effects_suppressed);

    // Aliases into ws and roster were bound by the constructor. Do not copy the
    // source controller, mutex or external callbacks into the branch.
#define COPY(field) assign(dst.field, source.field)
    COPY(on_stage); COPY(roundCount); COPY(uuid); COPY(currentState); COPY(ws);
    COPY(plugin_storage); COPY(abnormal_status_end_round); COPY(anomaly_duration_cap);
    COPY(anomaly_resist_low_penalty); COPY(round_effect_valid_id); COPY(watcher_valid_id);
    COPY(pipeline_valid_id); COPY(trait_state_); COPY(anomaly_view_layers); COPY(prev_round_preemptive_right);
    COPY(effect_arg_faults); COPY(effect_arg_faulted); COPY(effect_arg_fail_fast);
    COPY(effect_arg_reported_); COPY(effect_arg_faults_suppressed);
    COPY(hp_zero_converted); COPY(skill_use_seq); COPY(attack_boost_grants);
    COPY(crit_happened); COPY(force_execute_on_pp0); COPY(ignore_pp); COPY(pp_reverse);
    COPY(pp_retain_pet); COPY(cannot_act_round); COPY(hp_consume_marks);
    COPY(pending_skill_replacement); COPY(elf_element_view); COPY(elf_element_view_bound_slot);
    COPY(pink_reduce_pct); COPY(heal_mod_pct); COPY(anomaly_conversion); COPY(ability_levels);
    COPY(block_offstage_to_onstage); COPY(pet_death_notified); COPY(extra_spirits);
    COPY(is_boss_challenge); COPY(is_pvp_battle); COPY(standby_initialized);
    COPY(standby_pet_ids); COPY(standby_is_spirit_king); COPY(standby_hp_consume_kit);
    COPY(prev_on_stage_slot); COPY(prev_on_stage_elem); COPY(last_damage_actor); COPY(last_landed_hit);
    COPY(worn_equipment); COPY(active_suits); COPY(equip_stat_flat); COPY(equip_stat_pct);
    COPY(m_buffer); COPY(is_empty); COPY(debug_step_mode); COPY(breakpoints);
    COPY(current_player_id_); COPY(operation_log_); COPY(moves_log_); COPY(operation_collected);
    COPY(failed_attempts); COPY(max_rounds); COPY(finish_flag); COPY(finish_alive_snapshot);
    COPY(settlement_treated_alive); COPY(anomaly_rounds_frozen); COPY(host_side);
    COPY(deferred_switch_slot); COPY(forced_expel_pending);
#undef COPY
    error.clear();
    return std::unique_ptr<BattleCloneBranch>(new BattleCloneBranch(std::move(branch)));
}
