#ifndef SBS_LOCAL_BATTLE_CONTROL_H
#define SBS_LOCAL_BATTLE_CONTROL_H

#include <fsm/battle_clone.h>

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

    const BattleContext* context(Id id) const noexcept;
    // Closing a source also closes its attached shadow, not its ordinary clones.
    bool close(Id id);

private:
    struct Entry {
        std::unique_ptr<BattleCloneBranch> battle;
        Id shadow = 0;
        Id mirror_of = 0;
        std::uint64_t inputs = 0;
    };

    OpenResult insert(std::unique_ptr<BattleCloneBranch> battle, std::uint64_t inputs = 0);
    void detach(Entry& source);

    std::map<Id, Entry> entries_;
    Id next_id_ = 1;
};

#endif
