#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <fsm/state.h>

class ParsedEffectProgram;

// Prefixes belong to a single source decision: clones share the same immutable
// program. Retaining it prevents identity reuse even after a probe is discarded.
// Distinguish even converging edges of that program.
enum class BinaryChanceEdge { ConditionFailed, RollFailed, Success, Immune, Blocked, Other };

enum class ChanceSiteKind { Weighted, ParsedPercent, ForcedExpel, NativeSkill, SoulMark, Engine, Trait, Suit };
// Identifies engine rules, not synthetic plugin IDs. Give independent trials
// within one rule different subsites (e.g. the two critical-hit checks).
enum class EngineChanceRule {
    Accuracy, CriticalHit, Blinded, Distraction, MedicineTie, SpeedTie,
    AnomalyResistance, ComboCount, AnomalyApplication, RandomAnomaly,
    DerivedAnomaly, TicketsAnomaly, AnomalyImmunity, PeriodicAnomaly, SkillSeal,
};
enum class ChanceSingleton { SkipDraw, ConsumeDraw };

struct ChanceRange {
    int low;
    int high;
    bool operator==(const ChanceRange&) const = default;
};
// Existing rules keep their exact per-seed modulo mapping; new weighted rules
// reject an incomplete final group of tickets rather than biasing low IDs.
// LegacySlotRejection takes modulo the full ordered pool, retrying weight-zero
// slots; positive weights must be one. Disabled slots are never forced.
enum class ChanceDraw { Uniform, LegacyModulo, LegacySlotRejection };

struct ChanceOption {
    int id;
    std::uint32_t weight;
    bool operator==(const ChanceOption&) const = default;
};

struct BinaryChancePoint {
    std::shared_ptr<const ParsedEffectProgram> program;
    std::vector<int> arguments;
    int root_index = -1;
    int unit_index = -1;
    bool skip_root_condition = false;
    std::vector<BinaryChanceEdge> path;
    int round = 0;
    State state = State::GAME_START;
    int actor = -1;
    int target = -1;
    int percent = 0;  // Effective percentage, after the anomaly gate.

    ChanceSiteKind kind = ChanceSiteKind::Weighted;
    int rule_id = 0;  // NativeSkill/SoulMark: the registered effect/mark ID.
    int subsite_id = 0;  // Stable within that rule; not a per-cast invocation counter.
    ChanceDraw draw = ChanceDraw::Uniform;
    std::vector<ChanceOption> options;  // Natural draw order, including disabled (zero-weight) IDs.
    std::optional<ChanceRange> range;   // Mutually exclusive with options; all IDs have equal weight.

    bool operator==(const BinaryChancePoint&) const = default;
};

using ChancePoint = BinaryChancePoint;

inline ChancePoint chance_site(ChanceSiteKind kind, int rule, int subsite, int actor, int target) {
    ChancePoint site;
    site.kind = kind;
    site.rule_id = rule;
    site.subsite_id = subsite;
    site.actor = actor;
    site.target = target;
    return site;
}

inline ChancePoint engine_chance(EngineChanceRule rule, int subsite, int actor, int target) {
    return chance_site(ChanceSiteKind::Engine, static_cast<int>(rule), subsite, actor, target);
}
struct ChancePick {
    ChancePoint point;
    int outcome;  // Stable option ID, not an index.
};

class ChanceResolver {
public:
    virtual ~ChanceResolver() = default;
    // Only called for registered sites with multiple positive options. Select
    // by point identity alone, consistently across replays of the same root.
    virtual bool should_enumerate(const ChancePoint&) const { return true; }
    virtual int choose(const ChancePoint&) = 0;
    // A non-enumerated site used the ordinary RNG; outcome is the actual ID.
    virtual void record_sample(const ChancePoint&, int /*outcome*/) {}
};

struct BinaryChanceChoice {
    BinaryChancePoint point;
    bool outcome;
};

// Called synchronously on the disposable branch. A tree can reject a changed
// site by throwing BinaryChanceMismatch rather than forcing a different draw.
class BinaryPathResolver {
public:
    virtual ~BinaryPathResolver() = default;
    virtual bool choose(const BinaryChancePoint&) = 0;
};

struct BinaryChanceReplay {
    std::vector<BinaryChanceChoice> prefix;
    std::size_t cursor = 0;
    // Alternative to prefix replay. Borrowed only until the runner discards
    // this branch or clears its replay state; active replay cannot be cloned.
    BinaryPathResolver* resolver = nullptr;
    // Takes precedence over binary-only replay and sees native weighted sites too.
    ChanceResolver* weighted_resolver = nullptr;
    // Every low-level draw belongs to one of these categories, including
    // singleton legacy consumption and rejection retries.
    std::uint64_t classified_draws = 0; // Sites with semantic identities.
    std::uint64_t sampled_draws = 0;    // Explicit sample-only draws (not trace or tree entries).
    // First chance control exception, even if an effect catches it. The runner
    // must honor it before accepting any progress on this disposable branch.
    std::exception_ptr control_error;
};

// Control flow for the disposable probe, not a recoverable effect/argument error.
struct BinaryChanceNeeded { BinaryChancePoint point; };

struct BinaryChanceMismatch : std::runtime_error {
    using std::runtime_error::runtime_error;
};
