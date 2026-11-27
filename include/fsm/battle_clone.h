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
private:
    struct Impl;
    explicit BattleCloneBranch(std::unique_ptr<Impl>);
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
};

#endif
