#include <fsm/local_battle_control.h>

#include <fsm/battleContext.h>
#include <fsm/chance_dispatch.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

namespace {
LocalBattleControl::RunResult failed_run(std::string error) {
    LocalBattleControl::RunResult result;
    result.report.stop = BattleChanceStop::Failed;
    result.report.failed_mass = 1;
    result.report.diagnostic = std::move(error);
    return result;
}

std::string compare_step(const BattleContext& source, const BattleContext& shadow,
                         const BattleCloneResult& primary, const BattleCloneResult& replay) {
    if (primary.ok != replay.ok || primary.error != replay.error) {
        return "submit result differs (primary: " + (primary.ok ? "ok" : primary.error) +
               "; shadow: " + (replay.ok ? "ok" : replay.error) + ")";
    }
    if (source.currentState != shadow.currentState || source.roundCount != shadow.roundCount ||
        source.current_player_id_ != shadow.current_player_id_ || source.need_input() != shadow.need_input() ||
        source.operation_collected != shadow.operation_collected || source.failed_attempts != shadow.failed_attempts) {
        return "decision state differs";
    }
    if (source.finish_flag != shadow.finish_flag ||
        source.finish_alive_snapshot[0] != shadow.finish_alive_snapshot[0] ||
        source.finish_alive_snapshot[1] != shadow.finish_alive_snapshot[1]) {
        return "terminal state differs";
    }
    if (source.rng().state() != shadow.rng().state()) return "battle RNG differs";
    if (source.tape_.overflowed() || shadow.tape_.overflowed()) return "event tape reached its sample limit";

    const auto& a = source.tape_.samples();
    const auto& b = shadow.tape_.samples();
    const auto [left, right] = std::mismatch(a.begin(), a.end(), b.begin(), b.end());
    if (left != a.end() && right != b.end()) {
        return "event tape differs at sample " + std::to_string(left->seq) +
               " (state " + std::to_string(left->state) + ")";
    }
    if (left != a.end() || right != b.end() || source.tape_.next_seq() != shadow.tape_.next_seq()) {
        return "event tape length differs at sample " +
               std::to_string(left != a.end() ? left->seq : right != b.end() ? right->seq : source.tape_.next_seq());
    }
    if (source.getFullStateJson() != shadow.getFullStateJson()) return "diagnostic snapshot differs";
    return {};
}
}  // namespace

LocalBattleControl::OpenResult LocalBattleControl::insert(std::unique_ptr<BattleCloneBranch> battle,
                                                        std::uint64_t inputs) {
    if (next_id_ == 0) return {0, "local battle IDs exhausted"};
    const Id id = next_id_++;
    entries_.emplace(id, Entry{std::move(battle), 0, 0, inputs});
    return {id, {}};
}

LocalBattleControl::OpenResult LocalBattleControl::start(const std::array<SeerRobot, 2>& robots,
                                                        unsigned int seed, int max_rounds,
                                                        bool effect_arg_fail_fast) {
    if (dispatching_) return {0, "controller is dispatching a branch"};
    if (max_rounds < 0) return {0, "invalid round limit"};
    std::string error;
    auto battle = BattleCloneBranch::start_local(robots.data(), seed, max_rounds,
                                                 effect_arg_fail_fast, error);
    if (!battle) return {0, std::move(error)};
    return insert(std::move(battle));
}

LocalBattleControl::OpenResult LocalBattleControl::clone(Id source) {
    if (dispatching_) return {0, "controller is dispatching a branch"};
    const auto it = entries_.find(source);
    if (it == entries_.end()) return {0, "unknown local battle"};
    if (it->second.branch_managed) return {0, "automatic branches are managed by run()"};
    if (it->second.mirror_of) return {0, "mirror is read-only"};
    std::string error;
    auto branch = it->second.battle->clone(error);
    if (!branch) return {0, std::move(error)};
    // Local IDs do not deliver history from before the fork. Raw engine
    // clones still keep their tape, and disabling it does not reset its cursor.
    branch->context().enable_tape(false);
    return insert(std::move(branch), it->second.inputs);
}

LocalBattleControl::OpenResult LocalBattleControl::mirror(Id source) {
    if (dispatching_) return {0, "controller is dispatching a branch"};
    const auto it = entries_.find(source);
    if (it == entries_.end()) return {0, "unknown local battle"};
    if (it->second.branch_managed) return {0, "automatic branches are managed by run()"};
    auto& root = it->second;
    if (root.mirror_of) return {0, "mirror is read-only"};
    if (root.shadow) return {0, "local battle already has a mirror"};
    auto opened = clone(source);
    if (!opened) return opened;

    auto& shadow = entries_.at(opened.id);
    root.shadow = opened.id;
    shadow.mirror_of = source;
    auto& ctx = root.battle->context();
    ctx.tape_.clear_delivered();
    ctx.enable_tape(true);
    shadow.battle->context().enable_tape(true);
    return opened;
}

void LocalBattleControl::detach(Entry& source) {
    if (!source.shadow) return;
    entries_.at(source.shadow).battle->context().enable_tape(false);
    source.battle->context().enable_tape(false);
    source.shadow = 0;
}

LocalBattleControl::SubmitResult LocalBattleControl::submit(Id id, int player, BattleCloneAction action,
                                                           int index) {
    if (dispatching_) return {{false, "controller is dispatching a branch"}, std::nullopt};
    const auto it = entries_.find(id);
    if (it == entries_.end()) return {{false, "unknown local battle"}, std::nullopt};
    auto& source = it->second;
    if (source.auto_branching) return {{false, "use run() for automatic branching"}, std::nullopt};
    if (source.mirror_of) return {{false, "mirror is read-only"}, std::nullopt};
    const auto input = ++source.inputs;

    BattleCloneResult primary;
    try {
        primary = source.battle->submit(player, action, index);
    } catch (...) {
        detach(source);
        throw;
    }
    if (!source.shadow) return {std::move(primary), std::nullopt};

    const auto shadow_id = source.shadow;
    auto& branch = *entries_.at(shadow_id).battle;
    std::string detail;
    try {
        const auto replay = branch.submit(player, action, index);
        detail = compare_step(source.battle->context(), branch.context(), primary, replay);
    } catch (const std::exception& e) {
        detail = std::string("mirror step threw: ") + e.what();
    } catch (...) {
        detail = "mirror step threw an unknown exception";
    }
    if (!detail.empty()) {
        detach(source);
        return {std::move(primary), MirrorDifference{shadow_id, input, std::move(detail)}};
    }
    source.battle->context().tape_.clear_delivered();
    branch.context().tape_.clear_delivered();
    return {std::move(primary), std::nullopt};
}

BattleCloneResult LocalBattleControl::set_auto_branching(Id id, std::optional<BattleChanceOptions> options) {
    if (dispatching_) return {false, "controller is dispatching a branch"};
    const auto it = entries_.find(id);
    if (it == entries_.end()) return {false, "unknown local battle"};
    auto& entry = it->second;
    if (entry.shadow || entry.mirror_of) return {false, "automatic branching and mirroring are mutually exclusive"};
    if (options) {
        entry.auto_branching = std::make_shared<const BattleChanceOptions>(std::move(*options));
        entry.branch_managed = true;
    } else {
        entry.auto_branching.reset();
    }
    return {true, {}};
}

LocalBattleControl::RunResult LocalBattleControl::run(Id id, BattleInput input) {
    if (dispatching_) return failed_run("controller is dispatching a branch");
    const auto it = entries_.find(id);
    if (it == entries_.end()) return failed_run("unknown local battle");
    auto& source = it->second;
    if (!source.auto_branching) {
        auto result = submit(id, input.player, input.action, input.index);
        RunResult out;
        out.difference = std::move(result.difference);
        out.report.advances = 1;
        if (!result.primary) {
            out.report.stop = BattleChanceStop::Failed;
            out.report.failed_mass = 1;
            out.report.diagnostic = std::move(result.primary.error);
            return out;
        }
        out.report.leaves = 1;
        out.report.completed_mass = 1;
        out.branches.push_back({id, 1, 1, source.battle->context().finish_flag, {}, {}});
        return out;
    }
    if (!source.battle->context().need_input() || source.battle->context().finish_flag)
        return failed_run("battle is not waiting for input");

    // No controller mutations or user-visible leaves until all engine calls return.
    dispatching_ = true;
    struct ResetDispatching { bool& flag; ~ResetDispatching() { flag = false; } } reset{dispatching_};
    using namespace battle_chance_detail;
    std::vector<EngineLeaf<BattleCloneBranch>> done;
    const auto settings = source.auto_branching;
    RunResult out;
    out.report = dispatch<BattleCloneBranch>(*settings, EngineFailure::Diverged,
        [&](const BattleCloneBranch* entry, ResolverOwner& tape, const SaveBattle& save) {
            if (entry) return replay_input(*entry, std::nullopt, tape, save);
            return replay_input(*source.battle, input, tape, save);
        }, [&](EngineLeaf<BattleCloneBranch> leaf) { done.push_back(std::move(leaf)); });

    const auto complete = out.report.stop == BattleChanceStop::Complete;
    const auto describe = [&](Id leaf_id, EngineLeaf<BattleCloneBranch>& leaf) {
        out.branches.push_back({leaf_id, leaf.dispatch_id, leaf.mass, leaf.terminal,
                                std::move(leaf.trace), leaf.coverage});
    };
    out.branches.reserve(done.size());
    if (complete && done.size() == 1) {
        describe(id, done.front());
        source.battle = std::move(done.front().battle);
        ++source.inputs;
        return out;
    }
    if (!done.empty() && (next_id_ == 0 || done.size() - 1 > std::numeric_limits<Id>::max() - next_id_))
        return failed_run("local battle IDs exhausted");
    try {
        for (auto& leaf : done) {
            const auto opened = insert(std::move(leaf.battle), source.inputs + 1);
            auto& entry = entries_.at(opened.id);
            entry.auto_branching = settings;
            entry.branch_managed = true;
            describe(opened.id, leaf);
        }
    } catch (...) {
        for (const auto& leaf : out.branches) entries_.erase(leaf.id);
        throw;
    }
    if (complete) entries_.erase(it);
    return out;
}

std::optional<LocalBattleControl::View> LocalBattleControl::inspect(Id id) const {
    if (dispatching_) return std::nullopt;
    const auto it = entries_.find(id);
    if (it == entries_.end()) return std::nullopt;
    const auto& ctx = std::as_const(*it->second.battle).context();
    const std::array<int, 2> hp{ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp,
                                ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp};
    return View{ctx.currentState, ctx.roundCount, ctx.need_input() ? ctx.current_player_id_ : -1,
                ctx.finish_flag, hp, ctx.getStateJson()};
}

const BattleContext* LocalBattleControl::context(Id id) const noexcept {
    if (dispatching_) return nullptr;
    const auto it = entries_.find(id);
    return it != entries_.end() && !it->second.branch_managed ? &std::as_const(*it->second.battle).context() : nullptr;
}

bool LocalBattleControl::close(Id id) {
    if (dispatching_) return false;
    const auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    auto& entry = it->second;
    if (entry.shadow) entries_.erase(entry.shadow);
    if (entry.mirror_of) {
        const auto source = entries_.find(entry.mirror_of);
        if (source != entries_.end() && source->second.shadow == id) detach(source->second);
    }
    entries_.erase(it);
    return true;
}
