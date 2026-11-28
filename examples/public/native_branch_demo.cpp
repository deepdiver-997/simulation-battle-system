// A standalone library consumer: no official database or plugin is loaded.
#include "bare_pet.h"
#include "public_harness.h"

#include <fsm/local_battle_control.h>
#include <primitives/battle_primitives.h>

#include <iostream>

namespace {
using examples::check_eq;
using examples::check_true;
using Action = BattleCloneAction;
using Id = LocalBattleControl::Id;

// These IDs identify synthetic callbacks only within this executable.
constexpr int kCoinSkill = 91001;
constexpr int kArmOnSwap = 91002;

void coin_damage(BattleContext& ctx, int rule, int actor, int target) {
    const ChanceOption choices[] = {{17, 1}, {33, 3}};
    auto point = chance_site(ChanceSiteKind::NativeSkill, rule, 0, actor, target);
    const int damage = ctx.choose_weighted(choices, point);
    deal_true_damage(&ctx, target, damage, actor);
}

EffectResult coin_skill(BattleContext* ctx, const EffectArgs& args) {
    coin_damage(*ctx, kCoinSkill, args.int_args[0], args.int_args[1]);
    return EffectResult::kOk;
}

EffectResult arm_on_swap(BattleContext* ctx, const EffectArgs& args) {
    // Capture no outside state. Each cloned battle owns its own watcher.
    ctx->event_center_.register_watcher(EventType::EVENT_SWAP, args.int_args[0], ctx->roundCount, 0, true,
        [](BattleContext* battle, const BattleEvent& event) {
            coin_damage(*battle, kArmOnSwap, event.target, event.actor);
        }, WatcherScope::TEAM, 0, kArmOnSwap, BattleCallbackCopy::Audited);
    return EffectResult::kOk;
}

ElfPet demo_pet(int id, int effect_id, int speed) {
    auto skill = examples::skill_bare(91101, "Library example", 0, 100, 5, true);
    skill.type = SkillType::Attribute;
    if (effect_id) {
        auto effect = EffectFactory::getInstance().getEffect(effect_id, EffectArgs(std::vector<int>{0, 0}));
        skill.add_effect_node(SkillExecResult::HIT, {std::move(effect), State::BATTLE_FIRST_SKILL_EFFECT});
    }
    return examples::pet_bare(id, "Library pet", {skill, skill, skill, skill, skill}, 100, 500, speed);
}

LocalBattleControl::View inspect(const LocalBattleControl& ctl, Id id) {
    auto view = ctl.inspect(id);
    check_true(view.has_value(), "controller knows the branch ID");
    return std::move(*view);
}

BattleChanceOptions expand_only(int rule, std::size_t budget = 16) {
    return {budget, [rule](const ChancePoint& point) { return point.rule_id == rule; }};
}

void round_split() {
    LocalBattleControl ctl;
    const auto start = ctl.start({SeerRobot(examples::party_of(demo_pet(11, kCoinSkill, 120))),
                                  SeerRobot(examples::party_of(demo_pet(12, 0, 120)))}, 17);
    check_true(static_cast<bool>(start), start.error);
    const auto mirror = ctl.mirror(start.id);
    check_true(mirror && !ctl.set_auto_branching(start.id), "natural mirror and automatic branching are exclusive");
    check_true(ctl.close(mirror.id), "close the natural mirror");
    check_true(ctl.set_auto_branching(start.id, expand_only(kCoinSkill, 1)).ok, "enable native branching");

    const auto selected = ctl.run(start.id, {0, Action::SelectSkill, 0});
    check_true(selected.branches.size() == 1 && selected.branches.front().id == start.id,
               "a non-splitting first input keeps its ID");
    const auto waiting = inspect(ctl, start.id);
    check_eq(waiting.next_player, 1, "stop at the second seat's input wait");

    const auto rejected = ctl.run(start.id, {1, Action::SelectSkill, 99});
    check_true(rejected.report.stop == BattleChanceStop::Failed && rejected.branches.empty(), "reject invalid input");
    const auto cut = ctl.run(start.id, {1, Action::Skip});
    check_true(cut.report.stop == BattleChanceStop::BudgetExhausted && cut.branches.size() == 1 &&
        cut.report.completed_mass == 0.25L && cut.report.pending_mass == 0.75L, "report cut without publishing half a battle");
    check_true(inspect(ctl, start.id).state_json == waiting.state_json, "cut and rejection leave the original alone");
    check_true(ctl.close(cut.branches.front().id), "discard a completed child by ID");

    check_true(ctl.set_auto_branching(start.id, expand_only(kCoinSkill)).ok, "retry with a larger budget");
    auto split = ctl.run(start.id, {1, Action::Skip});
    check_true(split.report.stop == BattleChanceStop::Complete && split.branches.size() == 2 &&
        split.report.checkpoint_replays == 1, "both complete branches are returned; the sibling resumes at move-right");
    const auto& first = split.branches[0];
    const auto& second = split.branches[1];
    check_true(!ctl.inspect(start.id) && !ctl.context(first.id) && !ctl.context(second.id), "only the controller owns these IDs");
    check_true(first.dispatch_id == 1 && second.dispatch_id == 2 && first.id < second.id &&
        first.weight == 0.25L && second.weight == 0.75L, "keep declaration and dispatch order, not weight order");
    check_eq(inspect(ctl, first.id).active_hp[1], 483, "the first outcome applied its damage");
    const auto sibling = inspect(ctl, second.id);
    check_eq(sibling.active_hp[1], 467, "the second outcome independently applied its damage");
    const auto next = ctl.run(first.id, {0, Action::Skip});
    check_true(next.branches.size() == 1 && next.branches.front().id == first.id &&
        inspect(ctl, first.id).next_player == 1, "submit again to a selected branch by ID");
    check_true(inspect(ctl, second.id).state_json == sibling.state_json, "continuing one ID does not advance its sibling");
}

void first_input_split() {
    auto party = examples::party_of(demo_pet(11, 0, 110));
    party[1] = demo_pet(13, 0, 110);
    LocalBattleControl ctl;
    const auto start = ctl.start({SeerRobot(party), SeerRobot(examples::party_of(demo_pet(12, kArmOnSwap, 100)))}, 17);
    check_true(static_cast<bool>(start), start.error);
    check_true(ctl.submit(start.id, 0, Action::Skip).primary && ctl.submit(start.id, 1, Action::SelectSkill).primary,
               "arm the synthetic swap listener during an ordinary round");
    check_true(ctl.set_auto_branching(start.id, expand_only(kArmOnSwap)).ok, "enable native branching before the swap");
    auto split = ctl.run(start.id, {0, Action::ChoosePet, 1});
    check_true(split.report.stop == BattleChanceStop::Complete && split.branches.size() == 2,
               "the first seat's input already split in its event watcher");
    for (std::size_t i = 0; i < split.branches.size(); ++i) {
        const auto view = inspect(ctl, split.branches[i].id);
        check_eq(view.next_player, 1, "each branch independently waits for seat one");
        check_eq(view.active_hp[0], i == 0 ? 483 : 467, "swap effect changes only the entering pet in its own branch");
    }
}
}

int main() {
    // Registration is local to this program; no resource paths are consulted.
    auto& effects = EffectFactory::getInstance();
    effects.registerAuditedSkillEffect(kCoinSkill, &coin_skill, __FILE__);
    effects.registerAuditedSkillEffect(kArmOnSwap, &arm_on_swap, __FILE__);
    round_split();
    first_input_split();
    std::cout << "native branch demo PASS\n";
}
