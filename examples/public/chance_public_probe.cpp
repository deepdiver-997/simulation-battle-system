// examples/public/chance_public_probe.cpp — force both outcomes of a real FSM speed tie.
// Build with `cmake --build build --target sim_chance_public_probe`, then run
// `./build/sim_chance_public_probe`. No database, plugins, or training code are needed.
//
// LocalBattleControl owns the paused source; this single-threaded example has its
// exclusive lease and locks run_mutex while forking. Submit inputs only to owned,
// disposable children. A replay borrows its resolver: reset the replay before
// retaining a completed child, and destroy a mismatched child rather than resume
// it. A forced tie still advances the legacy RNG once. A faster-pet control runs
// the same skills without a tie, a resolver choice, or any RNG advance.
#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <fsm/battle_clone.h>
#include <fsm/binary_chance.h>
#include <fsm/local_battle_control.h>

#include "bare_pet.h"
#include "public_harness.h"

namespace {
using examples::check_eq;
using examples::check_true;

constexpr unsigned kSeed = 53;
constexpr int kSpeed = 200;
constexpr int kHp = 1000;
constexpr int kPP = 5;

SeerRobot make_robot(int id, const char* name, int speed) {
    // Guaranteed-hit attribute moves avoid accuracy and red damage draws.
    auto move = examples::skill_bare(910001, "Probe move", 0, 100, kPP, true);
    move.type = SkillType::Attribute;
    const std::array<Skills, 5> skills{move, move, move, move, move};
    return SeerRobot(examples::party_of(examples::pet_bare(id, name, skills, 100, kHp, speed)));
}

LocalBattleControl::Id open_root(LocalBattleControl& local, int right_speed) {
    const std::array robots{make_robot(910011, "A", kSpeed),
                            make_robot(910012, "B", right_speed)};
    const auto opened = local.start(robots, kSeed);
    check_true(static_cast<bool>(opened), "start seeded local battle: " + opened.error);
    // The public mirror API enables the source tape; submit nothing via the
    // local controller, so neither the source nor its diagnostic mirror runs a turn.
    const auto mirror = local.mirror(opened.id);
    check_true(static_cast<bool>(mirror), "enable source tape: " + mirror.error);
    return opened.id;
}

bool at_fresh_decision(const BattleContext& ctx) {
    return ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
           ctx.need_input() && ctx.operation_collected == std::array{false, false};
}

const ElfPet& active_pet(const BattleContext& ctx, int seat) {
    return ctx.seerRobot[seat].elfPets[ctx.on_stage[seat]];
}

struct Snapshot {
    std::uint64_t rng_state, rng_draws, rng_revision;
    int round;
    State state;
    bool needs_input, replay_active;
    std::array<bool, 2> collected;
    std::array<int, 2> hp, pp;
    bool tape_enabled;
    int tape_next_seq;
    std::vector<StateSample> tape;
    bool operator==(const Snapshot&) const = default;
};

Snapshot snapshot(const BattleContext& ctx) {
    return {ctx.rng().state(), ctx.rng().draw_count(), ctx.rng().revision(),
            ctx.roundCount, ctx.currentState, ctx.need_input(), ctx.binary_chance_replay.has_value(),
            ctx.operation_collected,
            {active_pet(ctx, 0).hp, active_pet(ctx, 1).hp},
            {active_pet(ctx, 0).skills[0].pp, active_pet(ctx, 1).skills[0].pp},
            ctx.tape_.enabled(), ctx.tape_.next_seq(), ctx.tape_.samples()};
}

class TieResolver final : public ChanceResolver {
public:
    TieResolver(int round, std::optional<int> forced_seat)
        : round_(round), forced_seat_(forced_seat) {}

    int choose(const ChancePoint& point) override {
        // Keep the default should_enumerate=true. An unexpected registered site
        // is a mismatch, not permission to sample it.
        if (++choices != 1 || !forced_seat_ || point.kind != ChanceSiteKind::Engine ||
            point.rule_id != static_cast<int>(EngineChanceRule::SpeedTie) || point.subsite_id != 0 ||
            point.state != State::BATTLE_FIRST_MOVE_RIGHT || point.round != round_ ||
            point.actor != -1 || point.target != -1 || point.range || point.options.size() != 2 ||
            std::find(point.options.begin(), point.options.end(), ChanceOption{0, 1}) == point.options.end() ||
            std::find(point.options.begin(), point.options.end(), ChanceOption{1, 1}) == point.options.end()) {
            throw BinaryChanceMismatch("expected exactly the two-seat speed tie");
        }
        return *forced_seat_;  // Stable option ID, not the position in point.options.
    }

    void record_sample(const ChancePoint&, int) override {
        throw BinaryChanceMismatch("this probe does not accept sampled sites");
    }

    int choices = 0;

private:
    int round_;
    std::optional<int> forced_seat_;
};

std::unique_ptr<BattleCloneBranch> play_child(const BattleContext& source,
                                              std::optional<int> forced_seat,
                                              bool expect_mismatch = false) {
    TieResolver resolver(source.roundCount, forced_seat);  // Outlives the disposable branch.
    std::string error;
    std::unique_ptr<BattleCloneBranch> child;
    {
        // LocalBattleControl lends a const view of an actually mutable battle.
        // Cast only the mutex for synchronization; never modify the source state.
        std::lock_guard lock(const_cast<std::mutex&>(source.run_mutex));
        // Mirroring can clone a half-collected round; this two-input example cannot.
        if (!at_fresh_decision(source) || source.binary_chance_replay) return nullptr;
        child = BattleClone::owned_at_decision(source, error);
    }
    check_true(child != nullptr, "clone the untouched decision: " + error);
    auto& ctx = child->context();
    auto& replay = ctx.binary_chance_replay.emplace();
    replay.weighted_resolver = &resolver;
    try {
        const auto left = child->submit(0, BattleCloneAction::SelectSkill, 0);
        if (replay.control_error) std::rethrow_exception(replay.control_error);
        check_true(left.ok, "submit A: " + left.error);
        check_true(ctx.need_input() && ctx.operation_collected == std::array{true, false},
                   "A's choice waits for B without resolving chance");
        check_eq(resolver.choices, 0, "no chance choice after only one submission");

        const auto right = child->submit(1, BattleCloneAction::SelectSkill, 0);
        if (replay.control_error) std::rethrow_exception(replay.control_error);
        check_true(right.ok, "submit B: " + right.error);
    } catch (const BinaryChanceMismatch& mismatch) {
        child.reset();  // May have consumed inputs and advanced the FSM: never retry it.
        check_true(expect_mismatch && resolver.choices == 1,
                   std::string("unexpected replay mismatch: ") + mismatch.what());
        return nullptr;
    }
    check_true(!expect_mismatch, "invalid outcome must reject this child");
    check_true(!ctx.effect_arg_faulted && ctx.event_center_.quiescent() &&
                   ctx.rng().revision() == source.rng().revision(), "the completed child is undisturbed");
    const int tie_draws = forced_seat.has_value() ? 1 : 0;
    check_eq(resolver.choices, tie_draws, "only a tied race is split");
    check_eq(replay.classified_draws, tie_draws, "legacy speed tie RNG consumption");
    check_eq(replay.sampled_draws, 0, "no sample-only RNG draws");
    check_true(ctx.rng().draw_count() == source.rng().draw_count() + tie_draws,
               "no other RNG consumption");
    if (!forced_seat) {
        check_true(ctx.rng().state() == source.rng().state(), "no split leaves RNG state unchanged");
    }
    ctx.binary_chance_replay.reset();  // Do not retain the resolver's borrowed pointer.
    return child;
}

void verify_completed(const BattleContext& child, int source_round, int first) {
    check_true(at_fresh_decision(child) && !child.binary_chance_replay,
               "completed child is back at a clean decision");
    check_eq(child.roundCount, source_round + 1, "one full round completed");
    for (int seat : {0, 1}) {
        check_eq(active_pet(child, seat).hp, kHp, "attribute move leaves HP unchanged");
        check_eq(active_pet(child, seat).skills[0].pp, kPP - 1, "each pet used one PP");
    }
    std::vector<int> actors;
    for (const auto& sample : child.tape_.samples()) {
        for (const auto& event : sample.events) {
            if (event.type == EventType::EVENT_SKILL_USED) actors.push_back(event.actor);
        }
    }
    check_true(actors == std::vector{first, 1 - first}, "skill events prove actual actor order");
}
}  // namespace

int main() {
    LocalBattleControl local;
    const auto tied_id = open_root(local, kSpeed);
    const auto& tied = *local.context(tied_id);
    const auto before = snapshot(tied);
    check_true(at_fresh_decision(tied) && !before.replay_active && before.tape_enabled &&
                   before.rng_state == kSeed && before.rng_draws == 0 && before.round == 0 &&
                   before.hp == std::array{kHp, kHp} && before.pp == std::array{kPP, kPP},
               "source is the initial seeded decision with neither input collected");

    auto left_first = play_child(tied, 0);
    verify_completed(left_first->context(), before.round, 0);
    const auto left_done = snapshot(left_first->context());
    auto right_first = play_child(tied, 1);
    verify_completed(right_first->context(), before.round, 1);
    const auto right_done = snapshot(right_first->context());
    check_true(&left_first->context() != &right_first->context() &&
                   &left_first->fsm() != &right_first->fsm() &&
                   &active_pet(left_first->context(), 0) != &active_pet(right_first->context(), 0) &&
                   snapshot(left_first->context()) == left_done,
               "completed siblings have disjoint state; running one does not change the other");
    check_true(snapshot(tied) == before, "both forced outcomes left the source unchanged");

    // The actual SpeedTie site has only IDs 0 and 1. Forcing ID 2 must fail closed.
    check_true(!play_child(tied, 2, true), "a mismatched replay returns no child");
    check_true(snapshot(tied) == before, "mismatch left source RNG, HP, PP, round and tape unchanged");
    check_true(local.close(tied_id), "close the source (and its diagnostic mirror)");
    check_true(snapshot(left_first->context()) == left_done && snapshot(right_first->context()) == right_done,
               "completed children outlive their source and their replay resolvers");
    check_true(left_first->submit(0, BattleCloneAction::SelectSkill, 0).ok,
               "an owned child can independently accept a new action");
    const auto partial = snapshot(left_first->context());
    check_true(!play_child(left_first->context(), 0) && snapshot(left_first->context()) == partial,
               "a half-collected round is not a two-input split root");
    check_true(snapshot(right_first->context()) == right_done, "advancing a child leaves its sibling unchanged");

    const auto faster_id = open_root(local, kSpeed + 1);
    const auto& faster = *local.context(faster_id);
    const auto faster_before = snapshot(faster);
    auto no_split = play_child(faster, std::nullopt);
    verify_completed(no_split->context(), faster_before.round, 1);
    check_true(snapshot(faster) == faster_before, "the no-split control left its source unchanged");

    std::cout << "chance public probe PASS: A then B; B then A; mismatch discarded; no-split drew nothing\n";
}
