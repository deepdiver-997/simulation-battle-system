#include <fsm/chance_dispatch.h>

#include <fsm/battleContext.h>

#include <algorithm>

namespace battle_chance_detail {
ReplayTape::ReplayTape(Trace specified, const BattleChanceFilter& filter)
    : trace_(std::move(specified)), specified_end_(trace_.size()), filter_(filter) {}

bool ReplayTape::should_enumerate(const ChancePoint& point) const {
    return !filter_ || filter_(point);
}

const BattleChanceRecord& ReplayTape::read(const ChancePoint& point, bool enumerated) {
    const auto& expected = trace_[next_];
    if (point != expected.point || enumerated != expected.enumerated)
        throw BinaryChanceMismatch("chance point changed on replay");
    ++next_;
    return expected;
}

int ReplayTape::choose(const ChancePoint& point) {
    if (next_ < specified_end_) return read(point, true).outcome;
    int first;
    if (point.range) {
        if (!point.options.empty() || point.range->low >= point.range->high)
            throw BinaryChanceMismatch("invalid compact chance range");
        first = point.range->low;
    } else {
        const auto live = [](const auto& option) { return option.weight != 0; };
        const auto option = std::find_if(point.options.begin(), point.options.end(), live);
        if (std::count_if(point.options.begin(), point.options.end(), live) < 2)
            throw BinaryChanceMismatch("chance split requires two positive options");
        for (auto pos = point.options.begin(); pos != point.options.end(); ++pos) {
            if (std::any_of(point.options.begin(), pos, [&](const auto& old) { return old.id == pos->id; }))
                throw BinaryChanceMismatch("duplicate chance outcome ID");
        }
        first = option->id;
    }
    trace_.push_back({point, first, true});
    return first;
}

void ReplayTape::record_sample(const ChancePoint& point, int outcome) {
    if (next_ < specified_end_) {
        if (read(point, false).outcome != outcome)
            throw BinaryChanceMismatch("natural sample changed on replay");
    } else {
        trace_.push_back({point, outcome, false});
    }
}

void ReplayTape::verify_complete() const {
    if (next_ != specified_end_) throw BinaryChanceMismatch("replay ended before the specified prefix");
}

void ReplayTape::start_segment() {
    trace_.clear();
    next_ = specified_end_ = 0;
}

ReplayScope::ReplayScope(BattleContext& battle, ResolverOwner* owner)
    : battle_(battle), return_to_(owner) {
    if (battle_.binary_chance_replay) throw std::logic_error("battle already has a chance replay");
    auto& replay = battle_.binary_chance_replay.emplace();
    if (owner) {
        replay.owned_resolver = std::move(*owner);
        replay.weighted_resolver = replay.owned_resolver.get();
    }
}

ReplayScope::~ReplayScope() { clear(); }

void ReplayScope::clear() noexcept {
    if (!battle_.binary_chance_replay) return;
    if (return_to_) *return_to_ = std::move(battle_.binary_chance_replay->owned_resolver);
    battle_.binary_chance_replay.reset();
}

std::unique_ptr<BattleCloneBranch> copy_at_pause(BattleCloneBranch& branch, std::string& error) {
    auto& slot = branch.context().binary_chance_replay;
    auto active = std::exchange(slot, std::nullopt);
    try {
        auto saved = branch.clone(error);
        slot = std::move(active);
        return saved;
    } catch (...) {
        slot = std::move(active);
        throw;
    }
}

BattleChanceCoverage measure_draws(const BattleContext& ctx, std::uint64_t from) {
    const auto& replay = *ctx.binary_chance_replay;
    const BattleChanceCoverage counts{ctx.rng().draw_count() - from, replay.classified_draws,
                                      replay.sampled_draws};
    if (counts.classified_draws > counts.engine_draws ||
        counts.sampled_draws > counts.engine_draws - counts.classified_draws)
        throw BinaryChanceMismatch("inconsistent battle RNG coverage");
    if (counts.unclassified_draws()) throw BinaryChanceMismatch("unclassified battle RNG draws during replay");
    return counts;
}

void add_coverage(BattleChanceCoverage& total, BattleChanceCoverage settled) {
    total.engine_draws += settled.engine_draws;
    total.classified_draws += settled.classified_draws;
    total.sampled_draws += settled.sampled_draws;
}

std::uint64_t range_count(ChanceRange range) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(range.high) - range.low + 1);
}

long double discover_siblings(const ReplayTape& tape, long double mass, const Enqueue& enqueue) {
    const auto& trace = tape.trace();
    for (auto i = tape.specified_end(); i < trace.size(); ++i) {
        const auto& [point, picked, enumerated] = trace[i];
        if (!enumerated) continue;
        const auto offer = [&](int outcome, long double weight, std::optional<ChanceRange> range = {}) {
            Trace prefix(trace.begin(), trace.begin() + i + 1);
            prefix.back().outcome = outcome;
            enqueue(std::move(prefix), weight, range);
        };
        if (point.range) {
            const auto [low, high] = *point.range;
            mass /= range_count(*point.range);
            offer(low + 1, mass, ChanceRange{low + 1, high});
            continue;
        }
        std::uint64_t total = 0;
        for (const auto& option : point.options) total += option.weight;
        const auto parent_mass = mass;
        for (const auto& [id, weight] : point.options) {
            if (!weight) continue;
            const auto share = parent_mass * (static_cast<long double>(weight) / total);
            if (id == picked) mass = share;
            else offer(id, share);
        }
    }
    return mass;
}

Attempt<BattleCloneBranch, EngineFailure> replay_input(const BattleCloneBranch& source,
                                                        std::optional<BattleInput> input,
                                                        ResolverOwner& tape, const SaveBattle& save) {
    using Result = Attempt<BattleCloneBranch, EngineFailure>;
    const auto fail = [](EngineFailure reason, std::string error) -> Result {
        return {nullptr, false, reason, std::move(error), {}};
    };
    std::string error;
    auto battle = source.clone(error); // Independent state at the exact decision or handler entry.
    if (!battle) return fail(EngineFailure::UnsafePause, std::move(error));
    auto& ctx = battle->context();
    ReplayScope replay(ctx, &tape);
    const auto draw_start = ctx.rng().draw_count();
    const auto revision = ctx.rng().revision();
    const auto check = [&](const auto& advance) {
        BattleCloneResult outcome;
        try {
            outcome = advance();
        } catch (...) {
            if (auto fault = ctx.binary_chance_replay->control_error) std::rethrow_exception(fault);
            throw;
        }
        if (auto fault = ctx.binary_chance_replay->control_error) std::rethrow_exception(fault);
        if (ctx.rng().revision() != revision) throw BinaryChanceMismatch("battle RNG changed during replay");
        if (ctx.effect_arg_faulted) return BattleCloneResult{false, "effect argument fault during replay"};
        return outcome;
    };

    if (input) {
        const auto [player, action, index] = *input;
        // The second joint input can reach move-right. The first input (and a
        // death replacement) uses the normal stop at the next input boundary.
        const bool stops_before_move_right = ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT &&
            player >= 0 && player < 2 && ctx.operation_collected[1 - player];
        const auto accepted = check([&] {
            return stops_before_move_right ? battle->step(player, action, index)
                                           : battle->submit(player, action, index);
        });
        if (!accepted) return fail(EngineFailure::Rejected, accepted.error);
        if (stops_before_move_right) {
            for (int i = 0; i < 8 && !ctx.need_input() && !ctx.finish_flag &&
                            ctx.currentState != State::BATTLE_FIRST_MOVE_RIGHT &&
                            ctx.currentState != State::FINISHED; ++i) {
                const auto step = check([&] { return battle->step(); });
                if (!step) return fail(EngineFailure::Rejected, step.error);
            }
            if (!ctx.finish_flag && ctx.currentState == State::BATTLE_FIRST_MOVE_RIGHT) {
                if (auto entry = copy_at_pause(*battle, error))
                    save(std::move(entry), measure_draws(ctx, draw_start));
            }
            if (ctx.currentState != State::FINISHED) {
                const auto rest = check([&] { return battle->run(); });
                if (!rest) return fail(EngineFailure::Rejected, rest.error);
            }
        }
    } else {
        const auto rest = check([&] { return battle->run(); });
        if (!rest) return fail(EngineFailure::Rejected, rest.error);
    }
    const bool terminal = ctx.finish_flag && ctx.currentState == State::FINISHED;
    if (ctx.debug_step_mode || !ctx.breakpoints.empty() || !ctx.event_center_.quiescent() ||
        (!terminal && (ctx.finish_flag || !ctx.need_input())))
        return fail(EngineFailure::UnsafePause, "battle did not reach an input wait or terminal");
    const auto coverage = measure_draws(ctx, draw_start);
    replay.clear();
    return {std::move(battle), terminal, EngineFailure::None, {}, coverage};
}
}
