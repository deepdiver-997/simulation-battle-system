#ifndef SBS_LOCAL_BATTLE_CONTROL_H
#define SBS_LOCAL_BATTLE_CONTROL_H

#include <fsm/battle_clone.h>
#include <fsm/chance_branch.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>

class SeerRobot;

// In-process, synchronous controller. The caller serializes its methods and
// any use of the borrowed context; no sockets or work queues are involved.
class LocalBattleControl {
public:
    using Id = std::uint64_t;

    struct OpenResult {
        Id id = 0;
        std::string error;
        explicit operator bool() const noexcept { return id != 0; }
    };

    struct MirrorDifference {
        Id shadow;
        std::uint64_t input;
        std::string detail;
    };

    struct SubmitResult {
        BattleCloneResult primary;
        // Diagnostic only. The primary result is not rolled back or retried.
        std::optional<MirrorDifference> difference;
    };

    OpenResult start(const std::array<SeerRobot, 2>& robots, unsigned int seed,
                     int max_rounds = 0, bool effect_arg_fail_fast = false);
    OpenResult clone(Id source);
    // One read-only shadow per source. The shadow ID remains inspectable on
    // divergence until closed; it never accepts its own submissions.
    OpenResult mirror(Id source);
    SubmitResult submit(Id id, int player, BattleCloneAction action, int index = 0);

    struct Branch {
        Id id;
        std::size_t dispatch_id; // One-based within this run; use id for subsequent calls.
        long double weight;
        bool terminal;
        std::vector<BattleChanceRecord> trace;
        BattleChanceCoverage coverage;
    };
    struct RunResult {
        BattleChanceReport report;
        std::vector<Branch> branches;
        std::optional<MirrorDifference> difference;
    };
    struct View {
        State state;
        int round;
        int next_player; // -1 when there is no input wait.
        bool terminal;
        std::array<int, 2> active_hp;
        std::string state_json;
    };

    // Opt in for this ID and its descendants. nullopt turns off enumeration;
    // a branch-managed battle remains inspectable only via an owned view.
    BattleCloneResult set_auto_branching(Id id, std::optional<BattleChanceOptions> options = BattleChanceOptions{});
    // Consume one input, stopping at the next actual wait or terminal. Results
    // hold IDs and values only. Automatic branching preserves the input root on a cut or failure.
    RunResult run(Id id, BattleInput input);
    std::optional<View> inspect(Id id) const;

    // Legacy borrowed pointer, valid only until the next mutating call.
    // Unavailable for branch-managed battles, even after branching is disabled.
    const BattleContext* context(Id id) const noexcept;
    // Closing a source also closes its attached shadow, not its ordinary clones.
    bool close(Id id);

private:
    struct Entry {
        std::unique_ptr<BattleCloneBranch> battle;
        Id shadow = 0;
        Id mirror_of = 0;
        std::uint64_t inputs = 0;
        std::shared_ptr<const BattleChanceOptions> auto_branching{};
        bool branch_managed = false;
    };

    OpenResult insert(std::unique_ptr<BattleCloneBranch> battle, std::uint64_t inputs = 0);
    void detach(Entry& source);

    std::map<Id, Entry> entries_;
    Id next_id_ = 1;
    bool dispatching_ = false;
};

#endif
