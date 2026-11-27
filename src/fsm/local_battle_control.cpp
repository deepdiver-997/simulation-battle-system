#include <fsm/local_battle_control.h>

#include <fsm/battleContext.h>

#include <algorithm>
#include <exception>
#include <mutex>
#include <utility>

namespace {

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
    if (max_rounds < 0) return {0, "invalid round limit"};
    std::string error;
    auto battle = BattleCloneBranch::start_local(robots.data(), seed, max_rounds,
                                                 effect_arg_fail_fast, error);
    if (!battle) return {0, std::move(error)};
    return insert(std::move(battle));
}

LocalBattleControl::OpenResult LocalBattleControl::clone(Id source) {
    const auto it = entries_.find(source);
    if (it == entries_.end()) return {0, "unknown local battle"};
    if (it->second.mirror_of) return {0, "mirror is read-only"};
    auto& ctx = it->second.battle->context();
    std::string error;
    std::unique_ptr<BattleCloneBranch> branch;
    {
        std::lock_guard lock(ctx.run_mutex);
        branch = BattleClone::owned_at_decision(ctx, error);
    }
    if (!branch) return {0, std::move(error)};
    // Local IDs do not deliver history from before the fork. Raw engine
    // clones still keep their tape, and disabling it does not reset its cursor.
    branch->context().enable_tape(false);
    return insert(std::move(branch), it->second.inputs);
}

LocalBattleControl::OpenResult LocalBattleControl::mirror(Id source) {
    const auto it = entries_.find(source);
    if (it == entries_.end()) return {0, "unknown local battle"};
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
    const auto it = entries_.find(id);
    if (it == entries_.end()) return {{false, "unknown local battle"}, std::nullopt};
    auto& source = it->second;
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

const BattleContext* LocalBattleControl::context(Id id) const noexcept {
    const auto it = entries_.find(id);
    return it != entries_.end() ? &std::as_const(*it->second.battle).context() : nullptr;
}

bool LocalBattleControl::close(Id id) {
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
