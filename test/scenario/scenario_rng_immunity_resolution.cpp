// A probability ticket draws only while resolving a threat, not during a read.
// Drive the anomaly primitive directly; no FSM RNG binding is required.
#include "scenario_harness.h"

#include <primitives/battle_primitives.h>

int main() {
    const Skills empty(Skills::EmptyPetTag{});
    const std::array<Skills, 5> skills{empty, empty, empty, empty, empty};
    numerical_properties stats{};
    stats[NumericalPropertyIndex::HP] = 500;
    const ElfPet pet(1, "rng fixture", {0, 0}, 0, Gender::NONE, {}, {}, stats, 500,
                     0, 0, false, skills);
    const auto roster = scenario::make_party(pet);
    const SeerRobot robots[]{SeerRobot(roster), SeerRobot(roster)};
    scenario::StubControlBlock control;
    BattleContext ctx(&control, robots, false, 1u);
    ctx.roundCount = 1;
    const auto status = static_cast<int>(AbnormalStatusId::Poison);

    ctx.grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0, 1, 0, false,
                       EffectScope::ON_STAGE, 0, 1287, ImmunityTier::Modern, nullptr,
                       [](BattleContext* c, int) { return c->rng().next() % 100 < 25; });
    ctx.seed_rng(1);
    sbs_srand(10);
    const auto driver_before = sbs_rand_state();
    const auto battle_before = ctx.rng().state();
    CHECK(!ctx.is_immune(0, ImmunityType::ANOMALY, ctx.currentState, status),
          "a probability ticket is not deterministic immunity");
    CHECK_EQ(ctx.rng().state(), battle_before, "query does not draw from the battle");

    ctx.grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0, 0, 0, false,
                       EffectScope::ON_STAGE, 1, 274);
    CHECK(apply_anomaly(&ctx, 0, status, 2, 1) == ApplyAnomalyResult::TARGET_IMMUNE,
          "the count ticket absorbs the first threat");
    CHECK_EQ(ctx.rng().state(), battle_before, "count protection short-circuits the probability ticket");

    SbsRng expected = ctx.rng();
    CHECK_EQ(expected.next() % 100, 7, "the battle seed must pass the 25 percent test");
    CHECK(apply_anomaly(&ctx, 0, status, 2, 1) == ApplyAnomalyResult::TARGET_IMMUNE,
          "the second threat uses the battle RNG, not the compatibility stream");
    CHECK_EQ(ctx.rng().state(), expected.state(), "the ticket drew exactly once");
    CHECK_EQ(sbs_rand_state(), driver_before, "direct primitive does not touch the compatibility RNG");
}
