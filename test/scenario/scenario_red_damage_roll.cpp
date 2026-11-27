// Red-damage range: vary only the existing formula draw (217..255), not
// hit, crit, effects, damage pipeline, or the following round.
#include <fsm/local_battle_control.h>
#include <numerical-calculation/calculation.h>

#include "chance_probe_mode.h"
#include "scenario_harness.h"

namespace {

using Action = BattleFsm::ActionType;

Skills attack(int power) {
    Skills skill(Skills::EmptyPetTag{});
    skill.id = 10001;
    skill.name = "range fixture";
    skill.type = SkillType::Physical;
    skill.power = power;
    skill.accuracy = 100;
    skill.must_hit = 1;
    skill.pp = skill.maxPP = 10;
    return skill;
}

ElfPet fighter(int id, int speed, const Skills& skill) {
    const Skills blank(Skills::EmptyPetTag{});
    numerical_properties stats{};
    stats[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    stats[NumericalPropertyIndex::DEFENSE] = 100;
    stats[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    stats[NumericalPropertyIndex::SPEED] = speed;
    stats[NumericalPropertyIndex::HP] = 1000;
    const std::array<Skills, 5> skills{skill, blank, blank, blank, blank};
    return ElfPet(id, "range fixture", {0, 0}, 0, Gender::NONE, {}, {},
                  stats, 1000, 0, 0, false, skills);
}

void check_range(const BattleWorkspace& ws, int side, int low, int high) {
    CHECK_EQ(ws.red_damage_roll_min[side], low, "red-damage lower bound");
    CHECK_EQ(ws.red_damage_roll_max[side], high, "red-damage upper bound");
}

void check_formula() {
    BattleWorkspace ws;
    const auto skill = attack(100);
    ws.battle_attrs[0][NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    ws.battle_attrs[1][NumericalPropertyIndex::DEFENSE] = 100;
    for (int variance = 217; variance <= 255; ++variance) {
        int draws = 0;
        const auto rolls = Calculation::calculateDamage(0, ws, skill, [&] { ++draws; return variance; });
        CHECK_EQ(rolls.sampled, 86 * variance / 255, "same sampled truncation at every bucket");
        CHECK_EQ(rolls.low, 73, "same low endpoint at every bucket");
        CHECK_EQ(rolls.high, 86, "same high endpoint at every bucket");
        CHECK_EQ(draws, 1, "endpoints never invoke the sampler");
    }
}

void check_menu_clone_and_round_reset() {
    const std::array<SeerRobot, 2> robots{
        SeerRobot(scenario::make_party(fighter(1, 200, attack(100)))),
        SeerRobot(scenario::make_party(fighter(2, 100, attack(50))))};
    LocalBattleControl control;
    const auto opened = control.start(robots, 33);
    CHECK(opened, opened.error);
    const auto& source = *control.context(opened.id);
    check_range(source.ws, 0, -1, -1);
    check_range(source.ws, 1, -1, -1);
    SbsRng expected_rng = source.rng();
    const int raw0 = 86 * (217 + expected_rng.next() % 39) / 255;
    const int raw1 = 44 * (217 + expected_rng.next() % 39) / 255;
    const auto submit = [&](LocalBattleControl::Id id, int side, BattleCloneAction action) {
        const auto result = control.submit(id, side, action);
        CHECK(result.primary.ok, result.primary.error);
    };
    submit(opened.id, 0, BattleCloneAction::SelectSkill);
    submit(opened.id, 1, BattleCloneAction::SelectSkill);
    CHECK(source.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT && source.need_input(),
          "round returns to the next decision menu");
    check_range(source.ws, 0, 73, 86);
    check_range(source.ws, 1, 37, 44);
    CHECK_EQ(source.ws.raw_attack_damage[0], raw0, "first side preserves its sampled damage");
    CHECK_EQ(source.ws.raw_attack_damage[1], raw1, "second side preserves its sampled damage");
    CHECK_EQ(source.rng().draw_count(), expected_rng.draw_count(), "one legacy draw per attack");

    const auto cloned = control.clone(opened.id);
    CHECK(cloned, cloned.error);
    const auto& branch = *control.context(cloned.id);
    check_range(branch.ws, 0, 73, 86);
    check_range(branch.ws, 1, 37, 44);
    submit(cloned.id, 0, BattleCloneAction::Skip);
    submit(cloned.id, 1, BattleCloneAction::Skip);
    CHECK_EQ(branch.roundCount, source.roundCount + 1, "the cloned battle advances independently");
    check_range(branch.ws, 0, -1, -1);
    check_range(branch.ws, 1, -1, -1);
    check_range(source.ws, 0, 73, 86);
    check_range(source.ws, 1, 37, 44);
}

struct StagedAttack {
    scenario::ScenarioBattle battle;

    explicit StagedAttack(const Skills& skill)
        : battle(scenario::make_party(fighter(1, 200, skill)),
                 scenario::make_party(fighter(2, 100, attack(50)))) {
        battle.start();
        battle.submit_operation(0, Action::SELECT_SKILL, 0);
        ctx().debug_step_mode = true;
        battle.submit_operation(1, Action::NONE, 0);
        for (int i = 0; i < 64 && battle.state() != State::BATTLE_FIRST_ATTACK_DAMAGE; ++i) {
            battle.step();
        }
        CHECK(battle.state() == State::BATTLE_FIRST_ATTACK_DAMAGE,
              "paused right before the first attack damage handler");
    }

    BattleContext& ctx() { return battle.ctx(); }
    void resolve() { battle.step(); }
};

void check_recalc() {
    StagedAttack staged(attack(100));
    auto& ctx = staged.ctx();
    auto& ws = ctx.ws;
    constexpr int def = static_cast<int>(NumericalPropertyIndex::DEFENSE);
    ctx.ability_level(1, def) = ws.view_levels[1][def] = 2;
    ctx.crit_happened[0] = true;
    ws.cached_crit_damage[0] = 175;
    ws.eff_crit_resist_pct[1] = 99;
    ws.base_resist_pct[1][0] = 35;
    int pierce_queries = 0;
    ctx.rule_center_.grant_attach_pierce(0, 1, kPierceResistBoost,
        [&](BattleContext*, int, int, bool) { ++pierce_queries; return true; });
    ctx.grant_attack_boost(0, 1, 1, 17);
    int combo_resolutions = 0;
    ws.combo_count_resolver[0] = [&](BattleContext*) { ++combo_resolutions; return 3; };
    ws.optimal_elem_rounds[0] = 1;
    ctx.prev_on_stage_slot[1] = 1;

    const auto scaled = [](int damage) { return damage * 117 / 100 * 175 / 100 * 65 / 100 * 3; };
    SbsRng expected_rng = ctx.rng();
    expected_rng.next(); // First formula uses boosted defense; only the second survives.
    const int raw = scaled(86 * (217 + expected_rng.next() % 39) / 255);
    staged.resolve();
    CHECK_EQ(combo_resolutions, 1, "dynamic combo is resolved once per attack");
    CHECK_EQ(pierce_queries, 2, "crit resistance pierce is queried once per formula pass");
    CHECK_EQ(ctx.ability_level(1, def), 0, "crit breaks defense before the recalculation");
    CHECK(ws.restraint_view[0] == -1.0, "temporary best-element override is restored");
    check_range(ws, 0, scaled(73), scaled(86));
    CHECK_EQ(ws.raw_attack_damage[0], raw, "recalculation replaces the sample and interval");
    CHECK_EQ(ctx.rng().draw_count(), expected_rng.draw_count(), "recalculation adds exactly one draw");
}

void check_missing_zero_and_overflow() {
    auto miss = attack(100);
    miss.accuracy = 0;
    miss.must_hit = 0;
    auto attribute = attack(0);
    attribute.type = SkillType::Attribute;
    for (const auto& skill : {miss, attribute}) {
        StagedAttack staged(skill);
        auto& ctx = staged.ctx();
        ctx.ws.red_damage_roll_min[0] = 21;
        ctx.ws.red_damage_roll_max[0] = 42;
        const auto draws = ctx.rng().draw_count();
        staged.resolve();
        check_range(ctx.ws, 0, -1, -1);
        CHECK_EQ(ctx.rng().draw_count(), draws, "miss and attribute paths do not draw variance");
    }
    {
        StagedAttack staged(attack(100));
        auto& ctx = staged.ctx();
        ctx.ws.restraint_view[0] = 0.0;
        const auto draws = ctx.rng().draw_count();
        staged.resolve();
        check_range(ctx.ws, 0, 0, 0);
        CHECK_EQ(ctx.rng().draw_count(), draws + 1, "a real zero formula still draws variance");
    }
    {
        StagedAttack staged(attack(24000000));
        auto& ctx = staged.ctx();
        ctx.seed_rng(39); // Next legacy draw is bucket 0; the upper endpoint overflows only the boost product.
        ctx.grant_attack_boost(0, 1, 1, 10);
        const int sampled_formula = static_cast<int>((0.84 * 24000000 + 2) * 217 / 255);
        staged.resolve();
        check_range(ctx.ws, 0, -1, -1);
        CHECK_EQ(ctx.ws.raw_attack_damage[0], sampled_formula * 110 / 100,
                 "hypothetical overflow does not fail or replace the live damage");
    }
}

}  // namespace

int main() {
    check_formula();
    // Probes deliberately do not add a new legacy trace chain.
    if (scenario::run_chance_probes()) {
        check_menu_clone_and_round_reset();
        check_recalc();
        check_missing_zero_and_overflow();
    }
}
