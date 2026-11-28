#ifndef SBS_BATTLE_CLONE_H
#define SBS_BATTLE_CLONE_H

#include <memory>
#include <string>

class BattleContext;
class BattleFsm;
class LocalBattleControl;
class SeerRobot;

// Source controller certifies exact ownership and no pending delivery or input.
// It is the caller's responsibility to hold the source lock and exclusive lease.
class BattleCloneControl {
public:
    virtual ~BattleCloneControl() = default;
    virtual bool clone_source_certified(const BattleContext&, const BattleFsm&) const noexcept = 0;
};

enum class BattleCloneAction { ChoosePet, SelectSkill, UseMedicine, Skip };
struct BattleInput {
    int player;
    BattleCloneAction action;
    int index = 0;
};
struct BattleCloneResult {
    bool ok = false;
    std::string error;
    explicit operator bool() const noexcept { return ok; }
};

// The caller serializes access to each branch. No Room or I/O is shared.
class BattleCloneBranch {
public:
    ~BattleCloneBranch();
    BattleCloneBranch(const BattleCloneBranch&) = delete;
    BattleCloneBranch& operator=(const BattleCloneBranch&) = delete;
    BattleContext& context() noexcept;
    const BattleContext& context() const noexcept;
    const BattleFsm& fsm() const noexcept;
    BattleCloneResult run();
    BattleCloneResult submit(int player, BattleCloneAction action, int index = 0);
    // Execute at most one state, optionally supplying a validated action first.
    BattleCloneResult step();
    BattleCloneResult step(int player, BattleCloneAction action, int index = 0);
    // Clone a stable input decision or a supported entry reached via step().
    // Serial use is required, but the caller must not hold run_mutex. Editing
    // execution state through context() invalidates the entry guarantee.
    std::unique_ptr<BattleCloneBranch> clone(std::string& error) const;
private:
    struct Impl;
    explicit BattleCloneBranch(std::unique_ptr<Impl>);
    BattleCloneResult advance(bool one_state);
    BattleCloneResult submit_action(int player, BattleCloneAction action, int index, bool one_state);
    static std::unique_ptr<BattleCloneBranch> start_local(
        const SeerRobot* robots, unsigned int seed, int max_rounds,
        bool effect_arg_fail_fast, std::string& error);
    std::unique_ptr<Impl> impl_;
    friend class BattleClone;
    friend class LocalBattleControl;
};

class BattleClone {
public:
    // The source must be paused waiting for input, with no queued work.
    // Caller holds source.run_mutex and the controller's exclusive lease.
    // This does not advance either branch, and cannot change the source.
    static std::unique_ptr<BattleCloneBranch> owned_at_decision(
        const BattleContext& source, std::string& error);
private:
    friend class BattleCloneBranch;
    static std::unique_ptr<BattleCloneBranch> owned_from_pause(
        const BattleContext& source, bool at_move_right_entry, std::string& error);
};

#endif
