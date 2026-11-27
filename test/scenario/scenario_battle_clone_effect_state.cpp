// Clone regressions using real moves plugins without assembling DB-backed skills.
// 1121 (j_pink_reduce_selfgate.cpp): independent, partially spent pink counters.
// 408 (round_window.cpp): refresh only one fork's flat reduction window.
// 1300 (tianqi_dijun_skills.cpp): refresh the red pipeline and TEAM watcher window
// together; compare RNG and anomalies across forks, after source destruction and
// through an active pet switch. Also exercise generic watcher copy admission.
#include <effects/effect.h>
#include <fsm/battle_clone.h>
#include <primitives/battle_primitives.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <mutex>
#include <string>
#include <thread>

#include "scenario_harness.h"
#include "chance_probe_mode.h"

namespace {
constexpr int kOwner = 0;
constexpr int kFoe = 1;
constexpr int kIncoming = 200;
constexpr int kPinkReduce = 1121;
constexpr int kFlatReduce = 408;
constexpr int kPrisonGuard = 1300;

void check_decision(const BattleContext& ctx) {
    CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
              ctx.need_input() && !ctx.finish_flag &&
              !ctx.operation_collected[kOwner] && !ctx.operation_collected[kFoe],
          "FSM is waiting at a fresh round decision");
}

ElfPet plain_pet(int id) {
    const Skills blank(Skills::EmptyPetTag{});
    const std::array<Skills, 5> skills{blank, blank, blank, blank, blank};
    constexpr int hp = 500;
    numerical_properties stats{};
    stats[NumericalPropertyIndex::HP] = hp;
    stats[NumericalPropertyIndex::SPEED] = 100;
    return ElfPet(id, "clone fixture", {0, 0}, 0, Gender::NONE, {}, {},
                  stats, hp, 0, 0, false, skills);
}

// This private, synchronous controller serves exactly one source. Its callbacks
// are no-ops and the test thread is its sole user, so no I/O or callback can race
// a clone, even outside the context's run_mutex.
class SourceBattle {
    struct Control final : scenario::StubControlBlock, BattleCloneControl {
        const BattleContext* bound_context = nullptr;
        const BattleFsm* bound_fsm = nullptr;

        bool clone_source_certified(const BattleContext& ctx,
                                    const BattleFsm& fsm) const noexcept override {
            return bound_context == &ctx && bound_fsm == &fsm &&
                   ctx.control_block_ == this && ctx.m_fsm == &fsm;
        }
    };

    Control control_;
    BattleFsm fsm_{false};
    SeerRobot robots_[2]{SeerRobot(scenario::make_party(plain_pet(1))),
                         SeerRobot(scenario::make_party(plain_pet(2)))};
    BattleContext context_{&control_, robots_, false, 1234u};

public:
    SourceBattle() {
        control_.bound_context = &context_;
        control_.bound_fsm = &fsm_;
        fsm_.run(&context_);
        check_decision(context_);
    }

    BattleContext& context() { return context_; }

    void skip(int side) {
        const int packet[] = {side, static_cast<int>(BattleFsm::ActionType::NONE), 0, 0};
        const auto* bytes = reinterpret_cast<const char*>(packet);
        context_.m_buffer.assign(bytes, bytes + sizeof(packet));
        context_.is_empty = false;
        fsm_.run(&context_);
    }

    std::unique_ptr<BattleCloneBranch> try_clone(std::string& error) {
        std::lock_guard<std::mutex> lock(context_.run_mutex);
        return BattleClone::owned_at_decision(context_, error);
    }

    std::unique_ptr<BattleCloneBranch> clone() {
        check_decision(context_);
        std::string error;
        std::unique_ptr<BattleCloneBranch> branch;
        {
            std::lock_guard<std::mutex> lock(context_.run_mutex);
            branch = BattleClone::owned_at_decision(context_, error);
        }
        CHECK(branch != nullptr, error);
        check_decision(branch->context());
        CHECK(&branch->fsm() != &fsm_ && branch->context().m_fsm == &branch->fsm() &&
                  branch->context().control_block_ != &control_,
              "branch owns an independent FSM and controller");
        const BattleCloneResult result = branch->run();
        CHECK(result.ok, result.error);
        check_decision(branch->context());
        CHECK_EQ(branch->context().roundCount, context_.roundCount,
                 "running an input wait does not advance the cloned round");
        return branch;
    }
};

void cast(EffectFn effect, BattleContext& ctx, int count_or_rounds, int reduction) {
    const EffectArgs args({kOwner, kFoe, count_or_rounds, reduction});
    CHECK(effect(&ctx, args) == EffectResult::kOk, "moves plugin effect failed");
}

int probe_pink(BattleContext& ctx, PinkDamageTier tier = PinkDamageTier::FIXED) {
    auto& pink = ctx.resolvedPink;
    const auto saved = pink;
    pink = {};
    pink.actor = kFoe;
    pink.target = kOwner;
    pink.tier = tier;
    pink.raw = pink.final = kIncoming;
    ctx.pink_damage_pipeline_.run(&ctx, kFoe, kOwner);
    const int result = pink.final;
    pink = saved;
    return result;
}

void check_pink(BattleContext& ctx, int expected, const char* message,
                PinkDamageTier tier = PinkDamageTier::FIXED) {
    CHECK_EQ(probe_pink(ctx, tier), expected, message);
}

int probe_red(BattleContext& ctx, DamagePhase phase = DamagePhase::REDUCE_FLAT,
              int incoming = kIncoming) {
    auto& red = ctx.resolvedDamage;
    const auto saved = red;
    red = {};
    red.attackerId = kFoe;
    red.defenderId = kOwner;
    red.isRed = true;
    red.final = incoming;
    ctx.damage_pipeline_.run(&ctx, kFoe, kOwner, DamagePipeline::phase_bit(phase));
    const int result = red.final;
    red = saved;
    return result;
}

void deliver_event(BattleContext& ctx, const BattleEvent& event) {
    ctx.event_center_.emit(event);
    ctx.event_center_.drain(&ctx, ctx.roundCount, ctx.watcher_valid_id);
}

void skip_round(BattleCloneBranch& branch) {
    const int round = branch.context().roundCount;
    for (int side : {kOwner, kFoe}) {
        const BattleCloneResult result = branch.submit(side, BattleCloneAction::Skip);
        CHECK(result.ok, result.error);
    }
    check_decision(branch.context());
    CHECK_EQ(branch.context().roundCount, round + 1, "two skips advance exactly one round");
}

void test_pink_counters(EffectFn effect) {
    std::unique_ptr<BattleCloneBranch> survivor;
    {
        SourceBattle source;
        auto& ctx = source.context();
        cast(effect, ctx, /*count=*/3, /*pct=*/50);
        cast(effect, ctx, /*count=*/2, /*pct=*/20);
        check_pink(ctx, 80, "1121: both registrations apply; one charge spent from each");

        auto spent_first = source.clone();
        survivor = source.clone();
        auto& fork = spent_first->context();
        check_pink(fork, 80, "1121: both copied counters still reduce percent damage",
                   PinkDamageTier::PERCENT);
        check_pink(fork, 100, "1121: only the 50% counter has a charge left");
        check_pink(fork, kIncoming, "1121: copied counts were not reset to cast values");
        check_pink(ctx, 80, "1121: exhausting the first fork did not spend source charges");
    }
    skip_round(*survivor);
    auto& fork = survivor->context();
    check_pink(fork, 80, "1121: surviving fork kept both counters after source destruction");
    check_pink(fork, 100, "1121: the two surviving registration budgets are independent");
    check_pink(fork, kIncoming, "1121: surviving fork exhausts its own copied budget");
}

void test_flat_window(EffectFn effect) {
    std::unique_ptr<BattleCloneBranch> refreshed;
    std::unique_ptr<BattleCloneBranch> unchanged;
    {
        SourceBattle source;
        auto& ctx = source.context();
        cast(effect, ctx, /*rounds=*/3, /*reduction=*/30); // [r, r+3)
        CHECK_EQ(probe_red(ctx), 170, "408: initial window starts immediately");
        refreshed = source.clone();
        unchanged = source.clone();
        skip_round(*refreshed); // r+1: still in the original window
        auto& fork = refreshed->context();
        CHECK_EQ(probe_red(fork), 170, "408: the clone starts with the original window");

        // Supply the move-order input for a second-mover recast. At r+1 the
        // new window is [r+2, r+4), with a different start and duration.
        const auto saved_order = fork.ws.preemptive_right;
        fork.ws.preemptive_right = PreemptiveRight::SEER_ROBOT_2;
        cast(effect, fork, /*rounds=*/2, /*reduction=*/70);
        fork.ws.preemptive_right = saved_order;
        CHECK_EQ(probe_red(fork), kIncoming, "408: new start replaces the old window's start");
        CHECK_EQ(probe_red(ctx), 170, "408: refreshing a fork did not change the source window");
    }

    skip_round(*refreshed); // r+2: both ranges would apply here if the recast stacked
    skip_round(*unchanged);
    skip_round(*unchanged); // r+2: still in the original range
    CHECK_EQ(probe_red(refreshed->context()), 170,
             "408: recast kept the first reduction value and does not stack");
    CHECK_EQ(probe_red(unchanged->context()), 170,
             "408: control fork retained its original reduction after source destruction");
    skip_round(*refreshed);
    skip_round(*unchanged); // r+3: original range expires, refreshed range stays active
    CHECK_EQ(probe_red(unchanged->context()), kIncoming, "408: control fork expires on its own boundary");
    CHECK_EQ(probe_red(refreshed->context()), 170, "408: recast extended only the refreshed fork");
    skip_round(*refreshed); // r+4
    CHECK_EQ(probe_red(refreshed->context()), kIncoming, "408: refreshed range expires at its new end");
}

void cast_prison_guard(EffectFn effect, BattleContext& ctx, int rounds,
                       int reduction, int threshold, int anomaly_count) {
    const EffectArgs args({kOwner, kFoe, rounds, reduction, threshold, anomaly_count});
    CHECK(effect(&ctx, args) == EffectResult::kOk, "1300: moves plugin effect failed");
}

struct GuardTrace {
    std::uint64_t rng_state;
    std::array<int, kOfficialAbnormalStatusSlotCount> anomalies{};
    bool operator==(const GuardTrace&) const = default;
};

GuardTrace check_guard(BattleContext& ctx, int expected_damage, bool expect_proc,
                       const char* message, int raw_damage = 300) {
    const int damage = probe_red(ctx, DamagePhase::REDUCE_PCT, raw_damage);
    CHECK_EQ(damage, expected_damage, message);
    CHECK(!ctx.has_any_active_abnormal_status(kFoe), "1300: probe starts with a clean foe");

    // Model just the hit: run the real red pipeline and deliver the resulting
    // damage event without a DB-assembled skill or spending either pet's HP.
    int& raw = ctx.ws.raw_attack_damage[kFoe];
    const int saved_raw = raw;
    raw = raw_damage;
    BattleEvent event{EventType::EVENT_TAKE_DAMAGE, kFoe, kOwner, damage,
                      static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)};
    event.damage_kind = static_cast<int>(DamageKind::NORMAL);
    auto expected_rng = ctx.rng();
    if (expect_proc) {
        // Two distinct picks at 100% and no resistance rolls on the fixture.
        expected_rng.next();
        expected_rng.next();
    }
    deliver_event(ctx, event);
    raw = saved_raw;
    CHECK_EQ(ctx.rng().state(), expected_rng.state(), message);
    CHECK(ctx.has_any_active_abnormal_status(kFoe) == expect_proc, message);

    GuardTrace trace{ctx.rng().state()};
    for (int id = 0; id < kOfficialAbnormalStatusSlotCount; ++id) {
        trace.anomalies[id] = ctx.get_abnormal_status_end_round(kFoe, id);
    }
    ctx.clear_on_stage_abnormal_statuses(kFoe);
    return trace;
}

void test_prison_guard_window(EffectFn effect) {
    std::unique_ptr<BattleCloneBranch> refreshed;
    std::unique_ptr<BattleCloneBranch> unchanged;
    std::unique_ptr<BattleCloneBranch> switched;
    {
        SourceBattle source;
        auto& ctx = source.context();
        cast_prison_guard(effect, ctx, 3, 50, 280, 2); // [r, r+3)
        check_guard(ctx, 140, false, "1300: damage equal to the raw threshold does not proc", 280);

        refreshed = source.clone();
        unchanged = source.clone();
        const auto initial = check_guard(ctx, 150, true,
                                        "1300: watcher uses raw 300, not reduced damage 150");
        auto& fork = refreshed->context();
        CHECK(check_guard(fork, 150, true, "1300: first fork starts in the original window") == initial,
              "1300: first fork reproduces source RNG and anomalies");
        CHECK(check_guard(unchanged->context(), 150, true, "1300: control fork starts in the original window") == initial,
              "1300: control fork reproduces source RNG and anomalies independently");
        switched = source.clone(); // same RNG position as the other forks

        skip_round(*refreshed); // r+1; a second-mover recast replaces the window with [r+2, r+4)
        const auto saved_order = fork.ws.preemptive_right;
        fork.ws.preemptive_right = PreemptiveRight::SEER_ROBOT_2;
        cast_prison_guard(effect, fork, 2, 70, 999, 1);
        fork.ws.preemptive_right = saved_order;
        CHECK_EQ(fork.event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE), 1,
                 "1300: recast did not add a second watcher");
        check_guard(fork, 300, false, "1300: the later start disables both halves at r+1");
        check_guard(ctx, 150, true, "1300: recasting the fork did not replace the source window");
    }

    skip_round(*unchanged); // r+1
    const auto control = check_guard(unchanged->context(), 150, true,
                                     "1300: unchanged fork still has both halves after source destruction");
    skip_round(*switched);
    auto& after_switch = switched->context();
    const int next_slot = (after_switch.on_stage[kOwner] + 1) % 6;
    const auto result = switched->submit(kOwner, BattleCloneAction::ChoosePet, next_slot);
    CHECK(result.ok, result.error);
    CHECK_EQ(after_switch.on_stage[kOwner], next_slot, "1300: owner actually switched pets");
    CHECK(check_guard(after_switch, 300, true, "1300: switch drops red reduction but keeps the TEAM watcher") == control,
          "1300: the surviving TEAM watcher produces the same RNG and anomalies after switching");

    skip_round(*refreshed); // r+2: both the old and the refreshed window would be active if stacked
    skip_round(*unchanged);
    check_guard(refreshed->context(), 150, true,
                "1300: both refreshed halves activate with the original parameters, without stacking");
    check_guard(unchanged->context(), 150, true, "1300: control fork is still inside its original window");
    skip_round(*refreshed);
    skip_round(*unchanged); // r+3
    check_guard(unchanged->context(), 300, false, "1300: both control halves expire at the original end");
    check_guard(refreshed->context(), 150, true, "1300: both refreshed halves survive the original end");
    skip_round(*refreshed); // r+4
    check_guard(refreshed->context(), 300, false, "1300: both refreshed halves expire together");
}

constexpr int kWatcherTraceSlot = 42;
const BattleEvent kWatcherEvent{EventType::EVENT_HIT, kOwner, kFoe, 17};

int& watcher_trace(BattleContext& ctx) {
    return std::any_cast<int&>(ctx.plugin_storage.at(kWatcherTraceSlot));
}

int register_trace_watcher(BattleContext& ctx, int digit, bool once = false) {
    return ctx.event_center_.register_watcher(
        kWatcherEvent.type, kOwner, ctx.roundCount, /*duration_rounds=*/0, once,
        [digit](BattleContext* battle, const BattleEvent&) {
            int& trace = watcher_trace(*battle);
            trace = trace * 10 + digit;
        },
        WatcherScope::ON_STAGE, ctx.watcher_valid_id[kOwner], /*source_effect_id=*/0,
        BattleCallbackCopy::Audited);
}

void test_audited_watchers() {
    SourceBattle source;
    auto& ctx = source.context();
    ctx.plugin_storage.emplace(kWatcherTraceSlot, 0);
    const int first = register_trace_watcher(ctx, 1);
    const int once = register_trace_watcher(ctx, 2, true);
    const int last = register_trace_watcher(ctx, 3);
    const int retired = register_trace_watcher(ctx, 9);
    ctx.event_center_.remove_watcher(retired); // leave a hole above every live ID

    auto branch = source.clone();
    auto& fork = branch->context();
    const auto& copied = fork.event_center_.watchers();
    CHECK(copied.size() == 3 && copied.contains(first) && copied.contains(once) && copied.contains(last),
          "clone preserves all live watcher IDs and does not restore a removed watcher");
    const int added = register_trace_watcher(fork, 4);
    CHECK_EQ(added, retired + 1, "clone preserves the next ID, including the removed slot");
    CHECK_EQ(ctx.event_center_.watch_count(kWatcherEvent.type), 3, "fork registration is local");

    deliver_event(fork, kWatcherEvent);
    CHECK_EQ(watcher_trace(fork), 1234, "clone delivers copied watchers in the original order, new one last");
    CHECK_EQ(watcher_trace(ctx), 0, "copied watchers write to their delivery context, not the source");
    CHECK(!copied.contains(once) && ctx.event_center_.watchers().contains(once),
          "once consumption removes only the fork's watcher ID");
    fork.event_center_.remove_watcher(first);
    watcher_trace(fork) = 0;
    deliver_event(fork, kWatcherEvent);
    CHECK_EQ(watcher_trace(fork), 34, "once and explicitly removed fork watchers do not run again");

    deliver_event(ctx, kWatcherEvent);
    CHECK_EQ(watcher_trace(ctx), 123, "source retains both watchers removed from the fork");
    watcher_trace(ctx) = 0;
    deliver_event(ctx, kWatcherEvent);
    CHECK_EQ(watcher_trace(ctx), 13, "source consumes its once watcher on its own first delivery");
}

void test_watcher_clone_rejections() {
    SourceBattle source;
    auto& ctx = source.context();
    auto& events = ctx.event_center_;
    const int unaudited = events.register_watcher(
        kWatcherEvent.type, kOwner, ctx.roundCount, 0, false,
        [](BattleContext*, const BattleEvent&) {});
    std::string error;
    CHECK(!source.try_clone(error) && error.find("unverified watcher") != std::string::npos,
          "watcher clone admission is opt-in even for a captureless callback");
    events.remove_watcher(unaudited);

    ctx.plugin_storage.emplace(kWatcherTraceSlot, 0);
    register_trace_watcher(ctx, 1);
    events.emit(kWatcherEvent);
    CHECK(!source.try_clone(error) && error.find("pending") != std::string::npos,
          "a pending event blocks cloning even with only audited watchers");
    CHECK_EQ(watcher_trace(ctx), 0, "clone rejection does not deliver the queued event");
    events.drain(&ctx, ctx.roundCount, ctx.watcher_valid_id);
    CHECK_EQ(watcher_trace(ctx), 1, "the original queued event is still delivered exactly once");
    CHECK(source.try_clone(error) != nullptr, error);
}

void test_parallel_branches(EffectFn effect) {
    std::array<std::unique_ptr<BattleCloneBranch>, 3> branches;
    {
        SourceBattle source;
        cast(effect, source.context(), /*count=*/3, /*pct=*/50);
        for (auto& branch : branches) branch = source.clone();
    }
    struct Trace {
        std::array<int, 5> damage{};
        std::array<int, 5> rolls{};
        int rounds = 0;
        bool operator==(const Trace&) const = default;
    };
    const auto advance = [](BattleCloneBranch& branch) {
        Trace trace;
        for (std::size_t i = 0; i < trace.damage.size(); ++i) {
            if (!branch.submit(kOwner, BattleCloneAction::Skip) ||
                !branch.submit(kFoe, BattleCloneAction::Skip)) break;
            trace.damage[i] = probe_pink(branch.context());
            trace.rolls[i] = branch.context().sample_legacy_range(
                0, std::numeric_limits<int>::max() - 1, ChanceSingleton::ConsumeDraw);
            ++trace.rounds;
        }
        return trace;
    };
    const Trace serial = advance(*branches[0]);
    Trace parallel[2];
    std::thread a([&] { parallel[0] = advance(*branches[1]); });
    std::thread b([&] { parallel[1] = advance(*branches[2]); });
    a.join();
    b.join();
    CHECK_EQ(serial.rounds, 5, "serial branch completed each round");
    CHECK((serial.damage == std::array{100, 100, 100, 200, 200}), "charges work across rounds");
    CHECK(parallel[0] == serial && parallel[1] == serial,
          "independently scheduled branches reproduce the serial RNG and effect trace");
}

void test_partial_decision_and_rejections() {
    SourceBattle source;
    auto& ctx = source.context();
    ctx.enable_tape(true);
    ctx.set_defeat_effects_suppressed(kOwner, true);
    ctx.getPet(kOwner).on_stage_storage[42] = 7;
    source.skip(kOwner);
    CHECK(ctx.operation_collected[kOwner] && !ctx.operation_collected[kFoe], "source has one pending choice");
    std::string error;
    auto branch = source.try_clone(error);
    CHECK(branch != nullptr, error);
    auto& fork = branch->context();
    CHECK(fork.operation_collected[kOwner] && !fork.operation_collected[kFoe], "clone keeps pending choice");
    CHECK_EQ(std::any_cast<int>(fork.getPet(kOwner).on_stage_storage.at(42)), 7, "copy value payload");
    fork.getPet(kOwner).on_stage_storage[42] = 8;
    CHECK_EQ(std::any_cast<int>(ctx.getPet(kOwner).on_stage_storage.at(42)), 7, "copied payload is local");
    CHECK(!branch->submit(kOwner, BattleCloneAction::Skip), "already collected side cannot submit again");
    CHECK(fork.event_center_.defeat_effects_suppressed[0] == ctx.event_center_.defeat_effects_suppressed[0],
          "event suppression is copied");
    const int initial_seq = ctx.tape_.next_seq();
    CHECK_EQ(fork.tape_.next_seq(), initial_seq, "copy tape sequence");

    const BattleEvent event{EventType::EVENT_HIT, kOwner, kFoe, 17};
    fork.event_center_.emit(event);
    CHECK(branch->submit(kFoe, BattleCloneAction::Skip), "clone completes pending round");
    CHECK_EQ(ctx.tape_.next_seq(), initial_seq, "fork did not advance source tape");
    ctx.event_center_.emit(event);
    source.skip(kFoe);
    CHECK_EQ(fork.roundCount, ctx.roundCount, "source and fork get to the same round");
    CHECK_EQ(fork.rng().state(), ctx.rng().state(), "same commands consume the same RNG stream");
    const auto& source_samples = ctx.tape_.samples();
    const auto& fork_samples = fork.tape_.samples();
    CHECK_EQ(fork_samples.size(), source_samples.size(), "source and fork capture the same number of samples");
    bool recorded = false;
    for (std::size_t i = 0; i < source_samples.size(); ++i) {
        const auto& a = source_samples[i];
        const auto& b = fork_samples[i];
        CHECK(a.seq == b.seq && a.state == b.state && a.round == b.round, "sample order is preserved");
        CHECK_EQ(a.events.size(), b.events.size(), "event sink was rebound to the fork");
        for (std::size_t j = 0; j < a.events.size(); ++j) {
            const auto& x = a.events[j];
            const auto& y = b.events[j];
            CHECK(x.type == y.type && x.actor == y.actor && x.target == y.target && x.amount == y.amount,
                  "source and fork record identical event payloads");
            recorded = recorded || (x.type == event.type && x.amount == event.amount);
        }
    }
    CHECK(recorded, "event tape test must observe the inserted event");

    const auto state = ctx.rng().state();
    auto aliased = std::make_shared<int>(3);
    ctx.plugin_storage[42] = aliased;
    CHECK(!source.try_clone(error) && error.find("plugin_storage[42]") != std::string::npos,
          "unknown any rejects with its slot");
    CHECK_EQ(*aliased, 3, "clone rejection does not mutate existing state");
    ctx.plugin_storage.erase(42);
    ctx.getPet(kOwner).soulMark.args.extra = aliased.get();
    CHECK(!source.try_clone(error) && error.find("arguments") != std::string::npos,
          "external extra pointer must not be copied");
    ctx.getPet(kOwner).soulMark.args.extra = nullptr;
    ctx.register_pink_damage_effect(PinkDamagePhase::REDUCE_EXTRA, kOwner,
                                    [](BattleContext*, int) {});
    CHECK(!source.try_clone(error) && error.find("unverified pink-damage") != std::string::npos,
          "empty capture is not an implicit admission");
    CHECK_EQ(ctx.rng().state(), state, "clone rejection never consumes source RNG");
}
} // namespace

int main(int argc, char** argv) {
    // Use the build's plugin, not the potentially stale copy in resources/.
    // A directory argument is available for custom build layouts.
    const auto plugin_dir = argc > 1 ? std::filesystem::path(argv[1])
        : std::filesystem::absolute(argv[0]).parent_path() / "plugins" / "moves_lib";
    auto& moves = EffectFactory::getInstance(plugin_dir.string());
    const EffectFn pink_reduce = moves.getEffect(kPinkReduce).logic;
    const EffectFn flat_reduce = moves.getEffect(kFlatReduce).logic;
    const EffectFn prison_guard = moves.getEffect(kPrisonGuard).logic;
    CHECK(pink_reduce && flat_reduce && prison_guard,
          "moves plugin must register 1121, 408 and 1300: " << plugin_dir);
    test_pink_counters(pink_reduce);
    test_flat_window(flat_reduce);
    test_prison_guard_window(prison_guard);
    test_audited_watchers();
    test_watcher_clone_rejections();
    // The shared D16 file cannot preserve independent worker order.
    if (scenario::run_chance_probes()) test_parallel_branches(pink_reduce);
    test_partial_decision_and_rejections();
}
