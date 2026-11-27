// A probability ticket draws only while resolving a threat, not during a read.
// Drive the anomaly primitive directly; no FSM RNG binding is required.
#include "scenario_harness.h"

#include <primitives/battle_primitives.h>

namespace {
template<class T>
concept ExposesRngStorage = requires(T& battle) { battle.rng_.next(); };

template<class T>
concept ExposesRawDraw = requires(T& battle) { battle.rng().next(); };

template<class T>
concept ExposesRawSeed = requires(T& battle) { battle.rng().seed(1u); };

template<class T>
concept AcceptsNullWeightedSite = requires(T& battle, std::span<const ChanceOption> options) {
    battle.choose_weighted(options, nullptr);
};

template<class T>
concept AcceptsNullBinarySite = requires(T& battle) { battle.roll_binary_percent(25, nullptr); };

static_assert(!ExposesRngStorage<BattleContext>);
static_assert(!ExposesRawDraw<BattleContext>);
static_assert(!ExposesRawSeed<BattleContext>);
static_assert(!AcceptsNullWeightedSite<BattleContext>);
static_assert(!AcceptsNullBinarySite<BattleContext>);
} // namespace

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
                       [](BattleContext* c, int) {
                           return c->roll_legacy_gate(25, 100, engine_chance(EngineChanceRule::AnomalyImmunity, 0, 0, 0),
                                                      ChanceSingleton::SkipDraw);
                       });
    ctx.seed_rng(1);
    const auto& replay = ctx.binary_chance_replay.emplace();
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
    CHECK_EQ(ctx.rng().draw_count(), expected.draw_count(), "the named gate preserves draw count");
    CHECK_EQ(replay.classified_draws, 1u, "only the resolving immunity ticket is classified");
    CHECK_EQ(replay.sampled_draws, 0u, "the immunity gate is not a sample-only draw");
    CHECK_EQ(sbs_rand_state(), driver_before, "direct primitive does not touch the compatibility RNG");
}
