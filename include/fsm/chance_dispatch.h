#pragma once

// Internal bridge shared by the in-process controller and decision adapters.
// Consumers of the library should use LocalBattleControl instead.
#include <fsm/battle_clone.h>
#include <fsm/chance_branch.h>

#include <deque>
#include <utility>

class BattleContext;
class BattleCloneBranch;

namespace battle_chance_detail {
using Trace = std::vector<BattleChanceRecord>;
using ResolverOwner = std::unique_ptr<ChanceResolver>;

class ReplayTape final : public ChanceResolver {
public:
    ReplayTape(Trace specified, const BattleChanceFilter& filter);
    bool should_enumerate(const ChancePoint&) const override;
    int choose(const ChancePoint&) override;
    void record_sample(const ChancePoint&, int) override;
    void verify_complete() const;
    void start_segment();
    const Trace& trace() const noexcept { return trace_; }
    std::size_t specified_end() const noexcept { return specified_end_; }

private:
    const BattleChanceRecord& read(const ChancePoint&, bool enumerated);
    Trace trace_;
    std::size_t next_ = 0;
    std::size_t specified_end_;
    const BattleChanceFilter& filter_;
};

// During an engine run the context owns the tape. Returning it also removes
// the resolver, so a completed battle can be advanced or cloned normally.
class ReplayScope {
public:
    ReplayScope(BattleContext&, ResolverOwner*);
    ~ReplayScope();
    ReplayScope(const ReplayScope&) = delete;
    ReplayScope& operator=(const ReplayScope&) = delete;
    void clear() noexcept;

private:
    BattleContext& battle_;
    ResolverOwner* return_to_;
};

// Copy an audited pause without copying the currently attached tape.
std::unique_ptr<BattleCloneBranch> copy_at_pause(BattleCloneBranch&, std::string& error);
BattleChanceCoverage measure_draws(const BattleContext&, std::uint64_t from);
std::uint64_t range_count(ChanceRange);
void add_coverage(BattleChanceCoverage&, BattleChanceCoverage);

// Sibling prefixes stop exactly at the replacement result. Equal-weight ranges
// are emitted as one group; the dispatcher advances their cursor lazily.
using Enqueue = std::function<void(Trace, long double, std::optional<ChanceRange>)>;
long double discover_siblings(const ReplayTape&, long double, const Enqueue&);

template<class Battle>
struct EngineLeaf {
    std::unique_ptr<Battle> battle;
    bool terminal = false;
    std::size_t dispatch_id = 0;
    long double mass = 0;
    Trace trace;
    BattleChanceCoverage coverage;
};

template<class Battle, class Failure>
struct Attempt {
    std::unique_ptr<Battle> child;
    bool terminal = false;
    Failure failure{};
    std::string error;
    BattleChanceCoverage coverage;
};

template<class Failure>
struct DispatchReport : BattleChanceReport {
    Failure failure{};
};

template<class Battle, class Failure, class Drive, class Visit>
DispatchReport<Failure> dispatch(const BattleChanceOptions& options, Failure mismatch,
                                 const Drive& drive, const Visit& visit) {
    struct Checkpoint {
        std::unique_ptr<Battle> battle;
        Trace history;
        BattleChanceCoverage coverage;
    };
    struct Job {
        std::shared_ptr<const Checkpoint> start;
        Trace prefix;
        long double mass = 1;
        std::optional<ChanceRange> range;
    };
    std::deque<Job> pending(1); // Null checkpoint means the decision root.
    DispatchReport<Failure> report;
    while (!pending.empty() && report.advances < options.max_advances) {
        Job job;
        auto& front = pending.front();
        if (front.range && front.range->low < front.range->high) {
            job = {front.start, front.prefix, front.mass, {}};
            front.prefix.back().outcome = ++front.range->low;
        } else {
            job = std::move(front);
            pending.pop_front();
        }
        const auto dispatch_id = ++report.advances;
        const auto origin = job.start;
        if (origin) ++report.checkpoint_replays;
        ResolverOwner owner = std::make_unique<ReplayTape>(std::move(job.prefix), options.filter);
        auto& tape = static_cast<ReplayTape&>(*owner);

        const auto enqueue_new = [&] {
            job.mass = discover_siblings(tape, job.mass, [&](Trace prefix, long double mass, auto range) {
                pending.push_back({job.start, std::move(prefix), mass, range});
            });
        };
        const auto full_trace = [&] {
            Trace trace = job.start ? job.start->history : Trace{};
            trace.insert(trace.end(), tape.trace().begin(), tape.trace().end());
            return trace;
        };
        // Called between FSM runs, after the working battle reached a safe entry.
        const auto save = [&](std::unique_ptr<Battle> battle, BattleChanceCoverage coverage) {
            tape.verify_complete();
            enqueue_new();
            job.start = std::make_shared<Checkpoint>(Checkpoint{std::move(battle), full_trace(), coverage});
            tape.start_segment();
        };
        Attempt<Battle, Failure> step;
        try {
            step = drive(origin ? origin->battle.get() : nullptr, owner, save);
            if (step.child) tape.verify_complete();
        } catch (const BinaryChanceMismatch& error) {
            step.child.reset();
            step.failure = mismatch;
            step.error = error.what();
        } catch (const BinaryChanceNeeded&) {
            step.failure = mismatch;
            step.error = "synchronous replay requested an external choice";
        }
        // A failed attempt can still have discovered siblings before the error.
        enqueue_new();
        if (!step.child) {
            report.stop = BattleChanceStop::Failed;
            report.failed_mass = job.mass;
            report.failure = step.failure;
            report.diagnostic = std::move(step.error);
            break;
        }
        if (origin) add_coverage(step.coverage, origin->coverage);
        visit(EngineLeaf<Battle>{std::move(step.child), step.terminal, dispatch_id, job.mass,
                                 full_trace(), step.coverage});
        ++report.leaves;
        report.completed_mass += job.mass;
    }
    if (report.stop != BattleChanceStop::Failed && !pending.empty()) report.stop = BattleChanceStop::BudgetExhausted;
    for (const auto& job : pending) report.pending_mass += job.mass * (job.range ? range_count(*job.range) : 1);
    return report;
}

enum class EngineFailure { None, Rejected, UnsafePause, Diverged };
using SaveBattle = std::function<void(std::unique_ptr<BattleCloneBranch>, BattleChanceCoverage)>;
Attempt<BattleCloneBranch, EngineFailure> replay_input(const BattleCloneBranch& source,
                                                        std::optional<BattleInput> input,
                                                        ResolverOwner&, const SaveBattle&);
}
