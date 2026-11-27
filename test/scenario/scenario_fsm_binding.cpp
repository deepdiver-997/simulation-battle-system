#include "scenario_harness.h"
#include <entities/pet_factory.h>
#include <stdexcept>

int main() {
    REQUIRE(PetFactory::initialize_runtime_data());
    const auto pet = scenario::make_pet(101, "test", {10037, 10037, 10037, 10037, 10037},
                                        100, 100, 500);
    auto party = scenario::make_party(pet);
    SeerRobot robots[] = {SeerRobot(party), SeerRobot(party)};
    scenario::StubControlBlock control;
    BattleContext ctx(&control, robots, false, 1u);
    BattleFsm correct(false), wrong(false);
    REQUIRE(ctx.m_fsm == nullptr);

    // Direct operation follows the documented caller-serialized contract.
    {
        std::lock_guard<std::mutex> lock(ctx.run_mutex);
        correct.operation(&ctx, 0, BattleFsm::ActionType::NONE, 0);
    }
    REQUIRE(ctx.m_fsm == &correct);
    const int choice = ctx.roundChoice[0][0];
    bool rejected = false;
    try {
        std::lock_guard<std::mutex> lock(ctx.run_mutex);
        wrong.operation(&ctx, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
    } catch (const std::logic_error&) { rejected = true; }
    REQUIRE(rejected && ctx.m_fsm == &correct && ctx.roundChoice[0][0] == choice);
    const auto state = ctx.currentState;
    rejected = false;
    try { wrong.run(&ctx); } catch (const std::logic_error&) { rejected = true; }
    REQUIRE(rejected && ctx.m_fsm == &correct && ctx.currentState == state);

    // Starting through run also binds, and nested operation must not deadlock.
    BattleContext ctx2(&control, robots, false, 1u);
    correct.run(&ctx2);
    REQUIRE(ctx2.m_fsm == &correct);
    const int input[4] = {0, static_cast<int>(BattleFsm::ActionType::NONE), 0, 0};
    ctx2.m_buffer.assign(reinterpret_cast<const char*>(input),
                         reinterpret_cast<const char*>(input) + sizeof(input));
    ctx2.is_empty = false;
    correct.run(&ctx2);
    REQUIRE(ctx2.roundChoice[0][0] == static_cast<int>(BattleFsm::ActionType::NONE));
    rejected = false;
    try {
        std::lock_guard<std::mutex> lock(ctx2.run_mutex);
        wrong.operation(&ctx2, 0, BattleFsm::ActionType::NONE, 0);
    } catch (const std::logic_error&) { rejected = true; }
    REQUIRE(rejected && ctx2.m_fsm == &correct);
    bool null_rejected = false;
    try { correct.run(nullptr); } catch (const std::invalid_argument&) { null_rejected = true; }
    REQUIRE(null_rejected);
    null_rejected = false;
    try { correct.operation(nullptr, 0, BattleFsm::ActionType::NONE, 0); }
    catch (const std::invalid_argument&) { null_rejected = true; }
    REQUIRE(null_rejected);
}
