#pragma once

#include <fsm/binary_chance.h>

#include <functional>
#include <string>

// Public values returned by LocalBattleControl. None borrow the working battle.
struct BattleChanceRecord {
    ChancePoint point;
    int outcome;      // Candidate ID, not a position in the candidate array.
    bool enumerated;  // False for a draw sampled naturally by this run.
    bool operator==(const BattleChanceRecord&) const = default;
};

struct BattleChanceCoverage {
    std::uint64_t engine_draws = 0;
    std::uint64_t classified_draws = 0;
    std::uint64_t sampled_draws = 0;
    std::uint64_t unclassified_draws() const noexcept { return engine_draws - classified_draws - sampled_draws; }
};

using BattleChanceFilter = std::function<bool(const ChancePoint&)>;

struct BattleChanceOptions {
    std::size_t max_advances = 128;
    // Empty means enumerate all semantic sites. Otherwise use a pure predicate;
    // do not call the controller from it. Unselected sites still draw normally.
    BattleChanceFilter filter;
};

enum class BattleChanceStop { Complete, BudgetExhausted, Failed };

struct BattleChanceReport {
    BattleChanceStop stop = BattleChanceStop::Complete;
    std::size_t advances = 0, leaves = 0;
    std::size_t checkpoint_replays = 0;
    long double completed_mass = 0, pending_mass = 0, failed_mass = 0;
    std::string diagnostic;
};
