// DB-backed clone regression: moves[37448] has four distinct roots for
// effect_info[1907]. Compare a real cast from the source and its fork, then
// isolate ownership by scheduling one unmodified parsed effect at round end.
// Also replay a factory-built match with moves[20010]/effect_info[4], including
// a voluntary switch, a fork of the switched branch, and concurrent casts.
#include <abnormal-system/abnormal-types.h>
#include <effects/spirit_lifecycle.h>
#include <entities/pet_factory.h>
#include <entities/soul_mark_manager.h>
#include <entities/suit_manager.h>
#include <fsm/battle_clone.h>
#include <fsm/local_battle_control.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "scenario_harness.h"
#include "chance_probe_mode.h"

namespace {
constexpr int kCaster = 0;
constexpr int kTarget = 1;
constexpr int kMove = 37448;
constexpr int kEffect = 1907;
constexpr int kBasicMove = 10001;  // A DB move with no side effects.
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kSilence = static_cast<int>(AbnormalStatusId::Silence);
constexpr State kSettlement = State::BATTLE_ROUND_END;

void check_decision(const BattleContext& ctx) {
    CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
              ctx.need_input() && !ctx.finish_flag &&
              !ctx.operation_collected[kCaster] && !ctx.operation_collected[kTarget],
          "expected a fresh round decision");
}

ElfPet fixture_pet(int pet_id, const Skills& move, int speed, int hp) {
    // Neutral panel and no soul, trait or suit. Load the move from the DB
    // without requiring pet learnability.
    const Skills blank(Skills::EmptyPetTag{});
    const std::array<Skills, 5> skills{move, blank, blank, blank, blank};
    numerical_properties stats{};
    stats[NumericalPropertyIndex::HP] = hp;
    stats[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    stats[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    stats[NumericalPropertyIndex::DEFENSE] = 100;
    stats[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    stats[NumericalPropertyIndex::SPEED] = speed;
    return ElfPet(pet_id, "clone fixture", {0, 0}, 0, Gender::NONE, {}, {},
                  stats, hp, 0, 0, false, skills);
}

SeerRobot fixture(int pet_id, const Skills& move, int speed, bool with_reserve = false) {
    const auto empty = PetFactory::create_empty_pet();
    const auto lead = fixture_pet(pet_id, move, speed, with_reserve ? 160 : 1200);
    const auto reserve = with_reserve ? fixture_pet(pet_id, move, speed, 160) : empty;
    return SeerRobot({lead, reserve, empty, empty, empty, empty});
}

class SourceBattle {
    // No asynchronous I/O; this test exclusively owns the controller and FSM.
    struct Control final : scenario::StubControlBlock, BattleCloneControl {
        const BattleContext* context = nullptr;
        const BattleFsm* fsm = nullptr;

        bool clone_source_certified(const BattleContext& ctx,
                                    const BattleFsm& engine) const noexcept override {
            return context == &ctx && fsm == &engine && ctx.control_block_ == this;
        }
    } control_;
    BattleFsm fsm_{false};
    BattleContext ctx_;

public:
    SourceBattle(const std::array<SeerRobot, 2>& robots, unsigned int seed)
        : ctx_(&control_, robots.data(), false, seed) {
        control_.context = &ctx_;
        control_.fsm = &fsm_;
        ctx_.enable_tape(true);
        fsm_.run(&ctx_);
        check_decision(ctx_);
    }

    explicit SourceBattle(unsigned int seed, bool with_reserve = false)
        : SourceBattle({fixture(1, Skills(kMove), 200),
                        fixture(4, Skills(kBasicMove), 100, with_reserve)}, seed) {}

    BattleContext& context() { return ctx_; }

    void submit(int side, BattleFsm::ActionType action, int index = 0) {
        const int packet[] = {side, static_cast<int>(action), index, 0};
        const auto* bytes = reinterpret_cast<const char*>(packet);
        ctx_.m_buffer.assign(bytes, bytes + sizeof(packet));
        ctx_.is_empty = false;
        fsm_.run(&ctx_);
    }

    std::unique_ptr<BattleCloneBranch> try_clone(std::string& error) {
        std::lock_guard<std::mutex> lock(ctx_.run_mutex);
        return BattleClone::owned_at_decision(ctx_, error);
    }

    std::unique_ptr<BattleCloneBranch> clone() {
        std::string error;
        auto branch = try_clone(error);
        CHECK(branch != nullptr, error);
        check_decision(branch->context());
        return branch;
    }
};

using HitNodes = std::vector<SkillEffectNode>;

const HitNodes& check_roots(const Skills& skill) {
    const auto units = skill.parsed_units();
    constexpr std::array statuses{static_cast<int>(AbnormalStatusId::Paralysis),
                                  kBurn, kSilence, static_cast<int>(AbnormalStatusId::Flammable)};
    CHECK_EQ(skill.id, kMove, "use the DB-assembled move");
    CHECK_EQ(units.size(), statuses.size(), "1907 occurs four times in this move");
    const auto hit = skill.effectBranches.find(SkillExecResult::HIT);
    CHECK(hit != skill.effectBranches.end(), "1907 is registered on hit");
    CHECK_EQ(hit->second.size(), statuses.size(), "all four parsed clauses are registered");
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        const Effect& effect = hit->second[i].effect;
        const auto& ref = effect.args.parsed_unit;
        CHECK_EQ(effect.id, kEffect, "all roots come from effect 1907");
        CHECK(Skills::is_parsed_unit_effect(effect) && !effect.args.extra && ref.program,
              "parsed executor carries an owned program rather than an extra pointer");
        CHECK_EQ(ref.root_index, static_cast<int>(i), "each DB clause has its own root");
        CHECK(ref.program->unit_at(ref.root_index) == &units[i], "root resolves in the skill program");
        CHECK(units[i].primary_tag == PrimitiveTag::Anomaly && units[i].param0 == statuses[i] &&
                  units[i].chance_value == 25,
              "each root retains its own DB anomaly and chance");
    }
    return hit->second;
}

void submit(BattleCloneBranch& fork, int side, BattleCloneAction action) {
    const auto result = fork.submit(side, action);
    CHECK(result.ok, result.error);
}

auto event_key(const BattleEvent& e) {
    return std::tie(e.type, e.actor, e.target, e.amount, e.state, e.grant_id, e.blocked,
                    e.slot, e.damage_kind, e.cause, e.stat_index, e.level_after, e.level_delta,
                    e.used_attribute, e.stat_loss_kind, e.converted_to, e.skill_id, e.skill_name,
                    e.chance_source, e.switch_cause);
}

void compare_tapes(const StateTape& expected, const StateTape& actual, std::size_t start) {
    const auto& a = expected.samples();
    const auto& b = actual.samples();
    CHECK(a.size() > start && b.size() == a.size(), "both FSMs recorded a new round");
    bool used_move = false;
    bool attached_silence = false;
    for (std::size_t i = start; i < a.size(); ++i) {
        CHECK(a[i].seq == b[i].seq && a[i].state == b[i].state && a[i].round == b[i].round &&
                  a[i].hp[0] == b[i].hp[0] && a[i].hp[1] == b[i].hp[1],
              "source and fork record the same FSM timing and HP");
        CHECK_EQ(a[i].events.size(), b[i].events.size(), "same events at each timing");
        for (std::size_t j = 0; j < a[i].events.size(); ++j) {
            const auto& event = a[i].events[j];
            CHECK(event_key(event) == event_key(b[i].events[j]), "event order and payload match");
            used_move |= event.type == EventType::EVENT_SKILL_USED && event.actor == kCaster &&
                         event.skill_id == kMove;
            attached_silence |= event.type == EventType::EVENT_ANOMALY_APPLIED &&
                                event.actor == kCaster && event.target == kTarget &&
                                event.amount == kSilence;
        }
    }
    CHECK(used_move && attached_silence, "the DB cast used PP and reached its third parsed root");
}

void test_same_action() {
    // With unequal speeds, seed 1234 hits and then passes the third 25% roll.
    SourceBattle source(1234u);
    auto& ctx = source.context();
    const auto& roots = check_roots(ctx.getPet(kCaster).skills[0]);
    const int pp = ctx.getPet(kCaster).skills[0].pp;
    const int round = ctx.roundCount;
    const auto rng = ctx.rng().state();
    const auto tape_start = ctx.tape_.samples().size();
    auto fork = source.clone();
    auto& cloned = fork->context();
    const auto& cloned_roots = check_roots(cloned.getPet(kCaster).skills[0]);
    CHECK(cloned_roots.front().effect.args.parsed_unit.program ==
              roots.front().effect.args.parsed_unit.program,
          "copying the skill shares its immutable program");
    CHECK_EQ(cloned.getPet(kCaster).skills[0].pp, pp, "clone preserves skill PP");
    CHECK_EQ(cloned.roundCount, round, "clone does not advance the decision");
    CHECK_EQ(cloned.rng().state(), rng, "clone preserves the RNG position");
    CHECK_EQ(cloned.tape_.samples().size(), tape_start, "clone keeps the tape prefix");

    submit(*fork, kCaster, BattleCloneAction::SelectSkill);
    submit(*fork, kTarget, BattleCloneAction::Skip);
    CHECK(ctx.roundCount == round && ctx.getPet(kCaster).skills[0].pp == pp &&
              ctx.rng().state() == rng && ctx.tape_.samples().size() == tape_start,
          "advancing the fork leaves source round, PP, RNG, and tape untouched");
    source.submit(kCaster, BattleFsm::ActionType::SELECT_SKILL);
    source.submit(kTarget, BattleFsm::ActionType::NONE);
    check_decision(ctx);
    check_decision(cloned);
    CHECK_EQ(ctx.roundCount, round + 1, "source completed the cast round");
    CHECK_EQ(cloned.roundCount, ctx.roundCount, "fork completed the same round");
    CHECK_EQ(ctx.getPet(kCaster).skills[0].pp, pp - 1, "source spent one PP");
    CHECK_EQ(cloned.getPet(kCaster).skills[0].pp, pp - 1, "fork spent one PP");
    CHECK(ctx.rng().state() != rng, "the cast actually drew from the RNG");
    CHECK_EQ(cloned.rng().state(), ctx.rng().state(), "same actions advance the RNG identically");
    for (int id = 0; id < kOfficialAbnormalStatusSlotCount; ++id) {
        CHECK_EQ(cloned.get_abnormal_status_end_round(kTarget, id),
                 ctx.get_abnormal_status_end_round(kTarget, id), "same final anomaly slots");
    }
    compare_tapes(ctx.tape_, cloned.tape_, tape_start);
}

struct TurnInput {
    int side;
    BattleFsm::ActionType source_action;
    BattleCloneAction fork_action;
    int index = 0;
};

TurnInput next_input(const BattleContext& ctx, bool quiet_caster = false) {
    using Action = BattleFsm::ActionType;
    CHECK(ctx.need_input() && !ctx.finish_flag, "script requires a decision");
    if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
        const int side = ctx.current_player_id_;
        CHECK(side == kTarget && ctx.on_stage[side] == 0 && ctx.seerRobot[side].elfPets[1].hp > 0,
              "only the target has a living reserve in slot 1");
        return {side, Action::CHOOSE_PET, BattleCloneAction::ChoosePet, 1};
    }
    CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT, "expected a skill decision");
    const int side = ctx.operation_collected[kCaster] ? kTarget : kCaster;
    CHECK(!ctx.operation_collected[side], "script must submit an uncollected side");
    if (side == kCaster && quiet_caster) return {side, Action::NONE, BattleCloneAction::Skip};
    const auto& pet = ctx.seerRobot[side].elfPets[ctx.on_stage[side]];
    CHECK(pet.skills[0].pp > 0, "fixture must reach a knockout before running out of PP");
    return {side, Action::SELECT_SKILL, BattleCloneAction::SelectSkill};
}

void apply_input(SourceBattle& battle, const TurnInput& input) {
    battle.submit(input.side, input.source_action, input.index);
}

void apply_input(BattleCloneBranch& battle, const TurnInput& input) {
    const auto result = battle.submit(input.side, input.fork_action, input.index);
    CHECK(result.ok, result.error);
}

nlohmann::json checkpoint(const BattleContext& ctx) {
    auto state = nlohmann::json::parse(ctx.getFullStateJson());
    state.erase("uuid");  // Independent starts have different match IDs.
    state["testRng"] = ctx.rng().state();
    state["testCollected"] = ctx.operation_collected;
    state["testFinished"] = ctx.finish_flag;
    state["testFinishAlive"] = {ctx.finish_alive_snapshot[0], ctx.finish_alive_snapshot[1]};
    return state;
}

void compare_match_step(const BattleContext& ctx, const nlohmann::json& expected,
                        const StateTape& baseline, std::size_t begin, std::size_t end,
                        std::size_t step, const char* branch) {
    const auto actual = checkpoint(ctx);
    CHECK(actual == expected, branch << " diverged at input " << step << ": "
                                    << nlohmann::json::diff(expected, actual).dump());
    const auto& samples = ctx.tape_.samples();
    CHECK_EQ(samples.size(), end, branch << " tape length at input " << step);
    for (std::size_t i = begin; i < end; ++i) {
        const auto& a = baseline.samples()[i];
        const auto& b = samples[i];
        CHECK(std::tie(a.seq, a.state, a.round, a.actor, a.pending_damage, a.resolved_damage) ==
                  std::tie(b.seq, b.state, b.round, b.actor, b.pending_damage, b.resolved_damage),
              branch << " timing/damage at sample " << i << ", input " << step);
        for (int side = 0; side < 2; ++side) {
            CHECK(std::tie(a.hp[side], a.max_hp[side], a.pet_id[side], a.pet_slot[side], a.pet_name[side]) ==
                      std::tie(b.hp[side], b.max_hp[side], b.pet_id[side], b.pet_slot[side], b.pet_name[side]) &&
                      std::ranges::equal(a.levels[side], b.levels[side]),
                  branch << " active pet at sample " << i << ", input " << step);
        }
        CHECK_EQ(b.events.size(), a.events.size(), branch << " event count at sample " << i);
        for (std::size_t e = 0; e < a.events.size(); ++e) {
            CHECK(event_key(a.events[e]) == event_key(b.events[e]),
                  branch << " event " << e << " at sample " << i << ", input " << step);
        }
    }
}

void test_whole_match(unsigned int seed) {
    // Finish a separate run before creating any forks. Otherwise a shared write
    // could change the reference run along with both branches and pass the diff.
    SourceBattle single(seed, true);
    auto& baseline = single.context();
    std::vector<TurnInput> inputs;
    std::vector<nlohmann::json> states{checkpoint(baseline)};
    std::vector<std::size_t> ends{baseline.tape_.samples().size()};
    while (!baseline.finish_flag && inputs.size() < 24) {
        inputs.push_back(next_input(baseline));
        apply_input(single, inputs.back());
        states.push_back(checkpoint(baseline));
        ends.push_back(baseline.tape_.samples().size());
    }
    CHECK(baseline.finish_flag && baseline.currentState == State::FINISHED &&
              baseline.finish_alive_snapshot[kCaster] == 1 && baseline.finish_alive_snapshot[kTarget] == 0,
          "seed " << seed << " must finish by knockout, not an input or round cap");

    SourceBattle source(seed, true);
    auto fork = source.clone();
    auto sibling = source.clone();
    std::unique_ptr<BattleCloneBranch> after_death;
    const auto compare = [&](const BattleContext& ctx, std::size_t step, const char* label) {
        const auto begin = step == 0 ? 0 : ends[step - 1];
        compare_match_step(ctx, states[step], baseline.tape_, begin, ends[step], step, label);
    };
    const auto& first = fork->context();
    CHECK(&first.roundChoice == &first.ws.roundChoice && &first.pendingDamage == &first.ws.pendingDamage,
          "clone binds BattleContext references to its own workspace");
    for (const auto& robot : first.seerRobot) {
        for (const auto& pet : robot.elfPets) {
            CHECK(&pet.hp == &pet.numericalProperties[NumericalPropertyIndex::HP],
                  "clone binds each ElfPet HP reference to its own stats");
        }
    }

    compare(source.context(), 0, "source");
    compare(fork->context(), 0, "fork");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (!sibling->context().finish_flag) {
            apply_input(*sibling, next_input(sibling->context(), true));
            compare(source.context(), i, "source after sibling input");
            compare(fork->context(), i, "fork after sibling input");
        }
        const auto& input = inputs[i];
        if (input.fork_action == BattleCloneAction::ChoosePet && !after_death) {
            std::string error;
            after_death = source.try_clone(error);
            CHECK(after_death, "clone at forced replacement: " << error);
            compare(after_death->context(), i, "death-switch fork before replacement");
        }
        apply_input(*fork, input);
        compare(fork->context(), i + 1, "fork");
        compare(source.context(), i, "source before applying input");
        apply_input(source, input);
        compare(source.context(), i + 1, "source");
        if (after_death) {
            apply_input(*after_death, input);
            compare(after_death->context(), i + 1, "death-switch fork");
        }
    }
    CHECK(after_death, "the match went through a forced replacement");
    CHECK(!sibling->context().finish_flag && sibling->context().getPet(kCaster).hp < 1200 &&
              sibling->context().getPet(kCaster).skills[0].pp == sibling->context().getPet(kCaster).skills[0].maxPP,
          "the sibling took damage and did not cast; it did not just follow the reference script");
}

constexpr int kSwordDance = 20010;
constexpr int kSelfStatEffect = 4;
constexpr int kAttackLevel = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kReservePet = 120;
constexpr int kReserveSlot = 1;
constexpr unsigned int kRegisteredSeed = 1234u;
constexpr std::size_t kFirstCastStep = 2;
constexpr std::size_t kSwitchStep = 4;
constexpr std::size_t kSecondCastStep = 6;
using Action = BattleFsm::ActionType;

std::array<SeerRobot, 2> registered_fixture() {
    std::array<BattlePetMessage, 6> caster{}, foe{};
    for (auto* party : {&caster, &foe}) {
        for (auto& pet : *party) pet.pet_id = -1;  // Factory sentinel for an empty slot.
    }
    // Use the DB base stats, full starting HP, and strict learnability checks.
    caster[0] = {.pet_id = 6, .chosen_skills_id = {kSwordDance, 10004, 10018, 10024, 10027},
                 .trust_bar = false};
    foe[0] = {.pet_id = 12, .chosen_skills_id = {10037, kBasicMove, 10008, 10009, 10033},
              .trust_bar = false};
    foe[kReserveSlot] = {.pet_id = kReservePet, .chosen_skills_id = {kBasicMove, 10088, 10297, 10354, 10355},
                         .trust_bar = false};
    return {SeerRobotFactory::create_robot(caster), SeerRobotFactory::create_robot(foe)};
}

Effect& sword_dance_effect(BattleContext& ctx) {
    auto& move = ctx.getPet(kCaster).skills[0];
    CHECK_EQ(move.id, kSwordDance, "clone fixture uses the factory-built move");
    const auto hit = move.effectBranches.find(SkillExecResult::HIT);
    CHECK(hit != move.effectBranches.end() && hit->second.size() == 1, "Sword Dance has one HIT effect");
    auto& effect = hit->second.front().effect;
    CHECK_EQ(effect.id, kSelfStatEffect, "DB move carries effect 4");
    CHECK(effect.logic && effect.logic == EffectFactory::getInstance().getEffect(kSelfStatEffect).logic &&
              !Skills::is_parsed_unit_effect(effect) && !effect.args.extra,
          "effect 4 must be the actual registered function, not a parsed or injected fallback");
    return effect;
}

bool is_sword_dance(const BattleEvent& event) {
    return event.type == EventType::EVENT_SKILL_USED && event.actor == kCaster &&
           event.skill_id == kSwordDance && event.used_attribute;
}

int count_events_since(const BattleContext& ctx, std::size_t begin, auto predicate) {
    int count = 0;
    const auto& samples = ctx.tape_.samples();
    for (std::size_t i = begin; i < samples.size(); ++i) {
        count += static_cast<int>(std::ranges::count_if(samples[i].events, predicate));
    }
    return count;
}

TurnInput registered_attack_input(const BattleContext& ctx) {
    CHECK(ctx.need_input() && !ctx.finish_flag, "factory match script requires a decision");
    if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
        CHECK(ctx.current_player_id_ == kTarget && ctx.on_stage[kTarget] == kReserveSlot &&
                  ctx.seerRobot[kTarget].elfPets[0].hp > 0,
              "only the switched-in reserve can need a replacement");
        return {kTarget, Action::CHOOSE_PET, BattleCloneAction::ChoosePet, 0};
    }
    CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT, "expected a skill decision");
    const int side = ctx.operation_collected[kCaster] ? kTarget : kCaster;
    const int index = side == kCaster ? 1 : (ctx.on_stage[kTarget] == kReserveSlot ? 2 : 0);
    CHECK(!ctx.operation_collected[side] && ctx.seerRobot[side].elfPets[ctx.on_stage[side]].skills[index].pp > 0,
          "use an available physical attack until knockout");
    return {side, Action::SELECT_SKILL, BattleCloneAction::SelectSkill, index};
}

int knockout_winner(const BattleContext& ctx) {
    if (!ctx.finish_flag || ctx.currentState != State::FINISHED) return -1;
    const int caster = ctx.settlement_alive_count(kCaster);
    const int target = ctx.settlement_alive_count(kTarget);
    if (caster > 0 && target == 0) return kCaster;
    if (target > 0 && caster == 0) return kTarget;
    return -1;
}

void test_registered_match() {
    // The foe uses its weaker Tackle on both cast rounds so the caster can
    // demonstrate +2 before the switch, then another +2 from the new checkpoint.
    constexpr std::array prefix{
        TurnInput{kCaster, Action::SELECT_SKILL, BattleCloneAction::SelectSkill},
        TurnInput{kTarget, Action::SELECT_SKILL, BattleCloneAction::SelectSkill, 1},
        TurnInput{kCaster, Action::NONE, BattleCloneAction::Skip},
        TurnInput{kTarget, Action::CHOOSE_PET, BattleCloneAction::ChoosePet, kReserveSlot},
        TurnInput{kCaster, Action::SELECT_SKILL, BattleCloneAction::SelectSkill},
        TurnInput{kTarget, Action::SELECT_SKILL, BattleCloneAction::SelectSkill},
    };
    constexpr std::size_t input_cap = 24;
    const auto robots = registered_fixture();
    SourceBattle single(robots, kRegisteredSeed);
    auto& baseline = single.context();
    const int initial_pp = baseline.getPet(kCaster).skills[0].pp;
    const auto initial_rng = baseline.rng().state();
    sword_dance_effect(baseline);
    std::vector<TurnInput> inputs;
    std::vector<nlohmann::json> states{checkpoint(baseline)};
    std::vector<std::size_t> ends{baseline.tape_.samples().size()};
    while (!baseline.finish_flag && inputs.size() < input_cap) {
        const auto step = inputs.size() + 1;
        inputs.push_back(step <= prefix.size() ? prefix[step - 1] : registered_attack_input(baseline));
        apply_input(single, inputs.back());
        if (step == kFirstCastStep || step == kSecondCastStep) {
            const int casts = step == kFirstCastStep ? 1 : 2;
            CHECK_EQ(baseline.getPet(kCaster).skills[0].pp, initial_pp - casts, "Sword Dance spends PP");
            CHECK_EQ(baseline.ability_levels[kCaster][kAttackLevel], 2 * casts, "effect 4 raises self Attack");
            CHECK_EQ(baseline.ability_levels[kTarget][kAttackLevel], 0, "effect 4 does not boost the foe");
            CHECK_EQ(count_events_since(baseline, ends.back(), is_sword_dance), 1, "one real 20010 cast");
            CHECK_EQ(count_events_since(baseline, ends.back(), [casts](const BattleEvent& event) {
                return event.type == EventType::EVENT_STAT_CHANGED && event.target == kCaster &&
                       event.stat_index == kAttackLevel && event.level_delta == 2 && event.level_after == 2 * casts;
            }), 1, "cast produced one observable +2 Attack change");
        }
        if (step == kSwitchStep) {
            check_decision(baseline);
            CHECK(baseline.on_stage[kTarget] == kReserveSlot && baseline.getPet(kTarget).id == kReservePet &&
                      baseline.seerRobot[kTarget].elfPets[0].hp > 0,
                  "the living foe voluntarily switched to pet 120");
            CHECK_EQ(count_events_since(baseline, ends.back(), [](const BattleEvent& event) {
                return event.type == EventType::EVENT_SWAP && event.actor == kTarget &&
                       event.switch_cause == static_cast<int>(SwitchCause::ACTIVE);
            }), 1, "one voluntary switch event");
        }
        states.push_back(checkpoint(baseline));
        ends.push_back(baseline.tape_.samples().size());
    }
    const int winner = knockout_winner(baseline);
    CHECK(inputs.size() > kSecondCastStep && winner >= 0 &&
              (baseline.max_rounds == 0 || baseline.roundCount < baseline.max_rounds) &&
              !baseline.tape_.overflowed(),
          "factory baseline must end in a decisive knockout with a full tape within " << input_cap << " inputs");
    CHECK(baseline.rng().state() != initial_rng, "the real match exercised the battle RNG");

    SourceBattle source(robots, kRegisteredSeed);
    auto fork = source.clone();
    std::array<std::unique_ptr<BattleCloneBranch>, 2> parallel;
    if (scenario::run_chance_probes()) parallel = {source.clone(), source.clone()};
    std::unique_ptr<BattleCloneBranch> reclone;
    const auto compare = [&](const BattleContext& ctx, std::size_t step, const char* label) {
        compare_match_step(ctx, states[step], baseline.tape_, step == 0 ? 0 : ends[step - 1],
                           ends[step], step, label);
    };
    compare(source.context(), 0, "factory source");
    compare(fork->context(), 0, "factory fork");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        apply_input(*fork, inputs[i]);
        compare(fork->context(), i + 1, "factory fork");
        if (reclone) {
            apply_input(*reclone, inputs[i]);
            compare(reclone->context(), i + 1, "post-switch reclone");
        }
        compare(source.context(), i, "factory source before applying input");
        apply_input(source, inputs[i]);
        compare(source.context(), i + 1, "factory source");
        if (i + 1 == kSwitchStep) {
            auto& switched = fork->context();
            check_decision(switched);
            std::string error;
            std::lock_guard lock(switched.run_mutex);
            reclone = BattleClone::owned_at_decision(switched, error);
            CHECK(reclone, "reclone the fork after the voluntary switch: " << error);
            compare(reclone->context(), i + 1, "post-switch reclone at creation");
        }
    }
    CHECK(reclone && knockout_winner(source.context()) == winner && knockout_winner(fork->context()) == winner &&
              knockout_winner(reclone->context()) == winner, "all serial replays agree on the knockout winner");

    // The concurrent workers assert on separate tapes; keep the shared D16 stream serial.
    if (!scenario::run_chance_probes()) return;

    // Rendezvous inside the first cast's event delivery, not merely before
    // thread launch. Install these unverified observers only after cloning.
    // Neither observer mutates battle state, consumes RNG, or emits an event.
    std::mutex mutex;
    std::condition_variable cv;
    std::array<bool, 2> arrived{};
    bool timed_out = false;
    std::array<int, 2> watchers{};
    for (std::size_t i = 0; i < parallel.size(); ++i) {
        auto& ctx = parallel[i]->context();
        compare(ctx, 0, "concurrent fork at creation");
        watchers[i] = ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, kCaster, ctx.roundCount, 0, false,
            [&, i, expected = &ctx](BattleContext* actual, const BattleEvent& event) {
                if (actual != expected || !is_sword_dance(event)) return;
                std::unique_lock lock(mutex);
                if (arrived[i]) return;
                arrived[i] = true;
                cv.notify_all();
                if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return arrived[0] && arrived[1]; })) {
                    timed_out = true;
                }
            }, WatcherScope::ON_STAGE, ctx.watcher_valid_id[kCaster]);
    }
    struct Replay {
        std::vector<std::pair<nlohmann::json, std::size_t>> checkpoints;
        std::string error;
    };
    std::array<std::future<Replay>, 2> workers;
    for (std::size_t i = 0; i < parallel.size(); ++i) {
        workers[i] = std::async(std::launch::async, [&, i] {
            // CHECK's scenario counter is not atomic; defer assertions to the caller.
            Replay replay;
            auto& branch = *parallel[i];
            for (const auto& input : inputs) {
                const auto result = branch.submit(input.side, input.fork_action, input.index);
                if (!result.ok) {
                    replay.error = result.error;
                    break;
                }
                if (replay.checkpoints.size() + 1 == kFirstCastStep) {
                    branch.context().event_center_.remove_watcher(watchers[i]);
                }
                const auto& ctx = branch.context();
                replay.checkpoints.emplace_back(checkpoint(ctx), ctx.tape_.samples().size());
            }
            return replay;
        });
    }
    const std::array replays{workers[0].get(), workers[1].get()};
    CHECK(arrived[0] && arrived[1] && !timed_out, "both branches must overlap inside the real 20010 cast");
    for (std::size_t i = 0; i < replays.size(); ++i) {
        const auto& replay = replays[i];
        CHECK_EQ(replay.checkpoints.size(), inputs.size(), "concurrent fork " << i << " submit error: " << replay.error);
        for (std::size_t step = 1; step < states.size(); ++step) {
            const auto& [state, tape_end] = replay.checkpoints[step - 1];
            CHECK(state == states[step], "concurrent fork " << i << " state/RNG at input " << step << ": "
                                           << nlohmann::json::diff(states[step], state).dump());
            CHECK_EQ(tape_end, ends[step], "concurrent fork " << i << " tape length at input " << step);
        }
        const auto& ctx = parallel[i]->context();
        compare_match_step(ctx, states.back(), baseline.tape_, 0, ends.back(), inputs.size(), "concurrent fork");
        CHECK_EQ(knockout_winner(ctx), winner, "concurrent fork agrees on the knockout winner");
    }
}

void test_registered_effect_rejection() {
    SourceBattle source(registered_fixture(), kRegisteredSeed);
    auto& ctx = source.context();
    auto& effect = sword_dance_effect(ctx);
    const auto state = checkpoint(ctx);
    const auto reject = [&](const char* reason) {
        std::string error;
        CHECK(source.try_clone(error) == nullptr && error.find("skill 20010") != std::string::npos,
              "effect 4 with " << reason << " must reject cloning the DB move: " << error);
        CHECK(checkpoint(ctx) == state, "rejected clone must not advance state or RNG");
    };

    // Keep the DB id and arguments, but substitute an unregistered function.
    const auto logic = effect.logic;
    effect.logic = +[](BattleContext*, const EffectArgs&) { return EffectResult::kOk; };
    reject("a different function");
    effect.logic = logic;

    // Keep the registered function, but borrow a payload the clone cannot own.
    const int opaque = 0;
    effect.args.extra = &opaque;
    reject("an opaque extra pointer");
    effect.args.extra = nullptr;

    // The real cast consumes its one-shot before the next decision. Schedule
    // a copy for round end to exercise the separate pending-bucket clone gate.
    CHECK_EQ(effect.left_round, 0, "effect 4 is a one-shot from the skill definition");
    auto pending = std::make_unique<ContinuousEffect>(effect, kSettlement, kCaster, 1, ctx.roundCount);
    pending->once_ = true;
    auto& pending_args = pending->getEffect()->args;
    ctx.register_skill_effect(kSettlement, kCaster, std::move(pending));
    pending_args.extra = &opaque;
    std::string error;
    CHECK(!source.try_clone(error) && error.find("timed effect") != std::string::npos,
          "an audited function still cannot borrow an opaque timed payload: " << error);
    pending_args.extra = nullptr;

    auto fork = source.clone();
    auto& cloned = fork->context();
    const int pp = cloned.getPet(kCaster).skills[0].pp;
    submit(*fork, kCaster, BattleCloneAction::Skip);
    submit(*fork, kTarget, BattleCloneAction::Skip);
    check_decision(cloned);
    CHECK(cloned.ability_levels[kCaster][kAttackLevel] == 2 && cloned.getPet(kCaster).skills[0].pp == pp &&
              cloned.skills_effects.empty_at(kSettlement, kCaster),
          "audited pending effect fires once in the clone without a skill cast");
    CHECK_EQ(ctx.ability_levels[kCaster][kAttackLevel], 0, "timed execution does not touch the source");
}

void test_pending_program_ownership() {
    std::weak_ptr<const ParsedEffectProgram> program;
    std::unique_ptr<BattleCloneBranch> fork;
    {
        SourceBattle source(1u);  // No speed tie or cast; first probability roll is 7 (<25).
        auto& ctx = source.context();
        auto& skill = ctx.getPet(kCaster).skills[0];
        auto pending = check_roots(skill)[1].effect;  // Use the nonzero Burn root.
        program = pending.args.parsed_unit.program;

        // Only the test trigger changes. Preserve official Effect::left_round,
        // its one-shot normalization, all DB args, and the anomaly duration.
        CHECK_EQ(pending.left_round, 0, "1907 is a one-shot skill effect");
        auto node = std::make_unique<ContinuousEffect>(std::move(pending), kSettlement,
                                                       kCaster, 1, ctx.roundCount);
        node->once_ = true;
        auto& args = node->getEffect()->args;
        ctx.register_skill_effect(kSettlement, kCaster, std::move(node));
        skill = Skills(kBasicMove);
        CHECK(skill.parsed_units().empty(), "the caster no longer carries the parsed program");

        const auto reject = [&] {
            std::string error;
            CHECK(source.try_clone(error) == nullptr && error.find("timed effect") != std::string::npos,
                  "invalid parsed timed effect must reject cloning: " << error);
        };
        const auto rng = ctx.rng().state();
        const int root = args.parsed_unit.root_index;
        args.parsed_unit.root_index = static_cast<int>(args.parsed_unit.program->units().size());
        reject();
        args.parsed_unit.root_index = root;
        const int external = 0;
        args.extra = &external;
        reject();
        args.extra = nullptr;
        CHECK_EQ(ctx.rng().state(), rng, "rejected clones do not advance the source RNG");

        fork = source.clone();
    }

    CHECK(!program.expired(), "the cloned bucket owns the program after source destruction");
    auto& ctx = fork->context();
    const auto state = nlohmann::json::parse(ctx.getFullStateJson());
    const auto key = std::to_string(static_cast<int>(kSettlement)) + "_p0";
    const auto& entries = state.at("skillEffects").at(key).at("effects");
    CHECK(entries.size() == 1 && entries.at(0).at("effectId") == kEffect,
          "fullstate exposes the cloned pending effect");
    CHECK(!ctx.has_active_abnormal_status(kTarget, kBurn), "Burn has not run yet");
    const auto tape_start = ctx.tape_.samples().size();
    submit(*fork, kCaster, BattleCloneAction::Skip);
    submit(*fork, kTarget, BattleCloneAction::Skip);
    check_decision(ctx);

    int applied = 0;
    const auto& samples = ctx.tape_.samples();
    for (std::size_t i = tape_start; i < samples.size(); ++i) {
        for (const auto& event : samples[i].events) {
            if (event.type != EventType::EVENT_ANOMALY_APPLIED) continue;
            CHECK(samples[i].state == static_cast<int>(kSettlement) && event.actor == kCaster &&
                      event.target == kTarget && event.amount == kBurn,
                  "the pending DB root executed at round end without a skill cast");
            ++applied;
        }
    }
    CHECK_EQ(applied, 1, "the pending effect ran exactly once");
    CHECK(ctx.has_active_abnormal_status(kTarget, kBurn), "the pending effect actually applied Burn");
    CHECK(ctx.skills_effects.empty_at(kSettlement, kCaster) && program.expired(),
          "removing the one-shot node also releases its program");
}

void test_local_control() {
    constexpr TurnInput cast{kCaster, Action::SELECT_SKILL, BattleCloneAction::SelectSkill};
    constexpr TurnInput tackle{kTarget, Action::SELECT_SKILL, BattleCloneAction::SelectSkill, 1};
    constexpr TurnInput skip{kCaster, Action::NONE, BattleCloneAction::Skip};
    LocalBattleControl control;
    const auto root = control.start(registered_fixture(), kRegisteredSeed, 1);
    CHECK(root, root.error);
    const auto fork = control.clone(root.id);
    CHECK(fork, fork.error);
    const auto ctx = [&](LocalBattleControl::Id id) -> const BattleContext& {
        const auto* battle = control.context(id);
        CHECK(battle, "missing local context " << id);
        return *battle;
    };
    const auto advance = [&](LocalBattleControl::Id id, const TurnInput& input) {
        const auto result = control.submit(id, input.side, input.fork_action, input.index);
        CHECK(result.primary.ok, result.primary.error);
        CHECK(!result.difference, "mirror input " << result.difference->input << ": " << result.difference->detail);
    };
    const auto caster_pp = [&](LocalBattleControl::Id id) {
        const auto& battle = ctx(id);
        return battle.seerRobot[kCaster].elfPets[battle.on_stage[kCaster]].skills[0].pp;
    };
    check_decision(ctx(root.id));
    CHECK_EQ(ctx(fork.id).uuid, ctx(root.id).uuid, "ordinary clones retain the match UUID");
    const auto initial = checkpoint(ctx(root.id));
    const int initial_pp = caster_pp(root.id);
    CHECK(checkpoint(ctx(fork.id)) == initial, "ordinary clone starts at the same decision");
    advance(fork.id, skip);
    const auto fork_partial = checkpoint(ctx(fork.id));
    CHECK(checkpoint(ctx(root.id)) == initial, "the clone can submit a different input without changing the source");

    advance(root.id, cast);
    const auto partial = checkpoint(ctx(root.id));
    CHECK(ctx(root.id).need_input() && ctx(root.id).operation_collected[kCaster] &&
              !ctx(root.id).operation_collected[kTarget] && partial != fork_partial,
          "the source is already waiting for the other side, with a different action from the clone");
    const auto shadow = control.mirror(root.id);
    CHECK(shadow, shadow.error);
    CHECK(root.id != fork.id && root.id != shadow.id && fork.id != shadow.id,
          "each context has its own registry ID");
    const auto initial_seq = ctx(root.id).tape_.next_seq();
    const auto compare_mirror = [&] {
        CHECK(checkpoint(ctx(root.id)) == checkpoint(ctx(shadow.id)), "mirror state and RNG match");
        CHECK_EQ(ctx(root.id).tape_.next_seq(), ctx(shadow.id).tape_.next_seq(), "mirror sample sequence matches");
    };
    CHECK(ctx(shadow.id).operation_collected[kCaster] && !ctx(shadow.id).operation_collected[kTarget],
          "mirror inherits the source action already collected; no input replay is needed");
    compare_mirror();

    const auto duplicate = control.mirror(root.id);
    const auto nested = control.mirror(shadow.id);
    const auto direct = control.submit(shadow.id, cast.side, cast.fork_action, cast.index);
    CHECK(!duplicate && !duplicate.error.empty() && !nested && !nested.error.empty(),
          "duplicate and nested mirrors are rejected");
    CHECK(!direct.primary.ok && !direct.primary.error.empty() && !direct.difference,
          "direct input to an attached shadow is rejected");
    CHECK(checkpoint(ctx(root.id)) == partial && ctx(root.id).tape_.next_seq() == initial_seq,
          "rejected operations do not advance the root");
    const auto repeated = control.submit(root.id, cast.side, cast.fork_action, cast.index);
    CHECK(!repeated.primary && !repeated.difference, "both branches reject the same duplicate input");
    compare_mirror();

    const auto mirrored_step = [&](const TurnInput& input) {
        const auto seq = ctx(root.id).tape_.next_seq();
        advance(root.id, input);
        compare_mirror();
        CHECK(ctx(root.id).tape_.next_seq() > seq, "mirrored input recorded samples before clearing the tape");
    };

    mirrored_step(tackle);
    CHECK_EQ(caster_pp(root.id), initial_pp - 1, "the mirrored cast spent PP");
    CHECK_EQ(ctx(root.id).ability_levels[kCaster][kAttackLevel], 2, "the mirrored cast ran audited effect 4");

    // The cap is checked at the next round start, after collecting both choices.
    mirrored_step(cast);
    mirrored_step(tackle);
    CHECK(ctx(root.id).finish_flag && ctx(root.id).currentState == State::FINISHED && ctx(root.id).roundCount == 1,
          "the mirrored battle finishes at the one-round cap");
    const auto finished = checkpoint(ctx(root.id));
    const auto finished_seq = ctx(root.id).tape_.next_seq();
    const auto terminal_fork = control.clone(root.id);
    CHECK(!terminal_fork && !terminal_fork.error.empty(), "terminal context cannot be cloned");
    CHECK(checkpoint(ctx(root.id)) == finished && ctx(root.id).tape_.next_seq() == finished_seq,
          "terminal clone rejection is non-mutating");
    CHECK(control.close(root.id) && !control.context(root.id) && !control.context(shadow.id),
          "closing the root removes the attached shadow too");
    CHECK(!control.clone(root.id) && !control.mirror(root.id) && !control.close(root.id) &&
              !control.submit(root.id, cast.side, cast.fork_action).primary,
          "closed IDs do not route to any remaining context");
    CHECK(checkpoint(ctx(fork.id)) == fork_partial, "ordinary clone kept its own partial decision after root close");
    advance(fork.id, tackle);
    advance(fork.id, skip);
    advance(fork.id, tackle);
    CHECK(ctx(fork.id).finish_flag && caster_pp(fork.id) == initial_pp && checkpoint(ctx(fork.id)) != finished,
          "the independent clone finishes after closing the root, with the outcome of its own inputs");
}

void test_local_control_unverified_effect() {
    constexpr int kSpark = 10007;
    constexpr int kNativeBurn = 12;
    const Skills spark(kSpark);
    const auto hit = spark.effectBranches.find(SkillExecResult::HIT);
    CHECK(hit != spark.effectBranches.end() && hit->second.size() == 1, "Spark has one HIT effect");
    const auto& effect = hit->second.front().effect;
    CHECK(effect.id == kNativeBurn && effect.logic &&
              effect.logic == EffectFactory::getInstance().getEffect(kNativeBurn).logic &&
              !Skills::is_parsed_unit_effect(effect) && !EffectFactory::is_clone_audited(effect),
          "Spark uses a real registered but unaudited native effect");

    LocalBattleControl control;
    const auto root = control.start({fixture(1, spark, 200), fixture(4, Skills(kBasicMove), 100)}, kRegisteredSeed);
    CHECK(root, root.error);
    const auto* ctx = control.context(root.id);
    CHECK(ctx && ctx->seerRobot[kCaster].elfPets[ctx->on_stage[kCaster]].skills[0].id == kSpark,
          "local root carries the unaudited move");
    check_decision(*ctx);
    const auto initial = checkpoint(*ctx);
    const auto seq = ctx->tape_.next_seq();
    const auto reject = [&](const LocalBattleControl::OpenResult& rejected) {
        CHECK(!rejected && rejected.error.find("skill 10007") != std::string::npos,
              "local fork must reject the unaudited move: " << rejected.error);
        CHECK(checkpoint(*ctx) == initial && ctx->tape_.next_seq() == seq, "failed fork is non-mutating");
    };
    reject(control.clone(root.id));
    reject(control.mirror(root.id));
    const auto result = control.submit(root.id, kCaster, BattleCloneAction::Skip);
    CHECK(result.primary.ok && !result.difference && ctx->operation_collected[kCaster],
          "the root still accepts input after a failed mirror attachment");
}
}  // namespace

int main(int argc, char** argv) {
    // Prime all three registries from this build before PetFactory can select
    // the deployed resources/ copies (which may have a different EffectArgs ABI).
    namespace fs = std::filesystem;
    const auto plugins = argc > 1 ? fs::absolute(argv[1])
        : fs::absolute(argv[0]).parent_path() / "plugins";
    CHECK(fs::is_directory(plugins / "moves_lib") && fs::is_directory(plugins / "soul_lib") &&
              fs::is_directory(plugins / "suit_lib"),
          "expected same-build plugin directories under " << plugins);
    auto& moves = EffectFactory::getInstance((plugins / "moves_lib").string());
    auto& souls = SoulMarkManager::getInstance((plugins / "soul_lib").string());
    auto& suits = SuitManager::getInstance((plugins / "suit_lib").string());
    CHECK(!moves.registered_effect_ids().empty() && !souls.registered_soulmark_ids().empty() &&
              !suits.registered_suit_ids().empty(),
          "same-build moves, soul and suit plugins must load");
    const auto db = argc > 2 ? argv[2] : official_data::kDefaultOfficialDatabasePath;
    CHECK(PetFactory::initialize_runtime_data(db), "official DB initialization failed: " << db);

    test_same_action();
    for (const unsigned int seed : {1u, 1234u, 20261126u}) {
        test_whole_match(seed);
    }
    test_pending_program_ownership();
    test_registered_match();
    test_registered_effect_rejection();
    test_local_control();
    test_local_control_unverified_effect();
}
