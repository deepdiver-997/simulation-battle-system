// K350：taskctl alloc 5230-5239。ROUND_END前后精确体力/六槽快照。
#include <array>
#include <algorithm>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
using Levels = std::array<int, 6>;
constexpr int kMaxHp = 6000;

ElfPet pet(bool marked, std::vector<int> args) {
    SoulMark sm(1877, "切尔诺", "", EffectArgs(std::move(args)));
    sm.monster_id = 4292;
    return make_pet(4292, "回血探针", {10001, 10001, 10001, 10001, 10001},
                    200, 1, kMaxHp, marked ? sm : SoulMark());
}

void run_case(const char* label, const Levels& levels, std::vector<int> args,
              int expected_gain, bool boss = false, int owner = 0,
              int hp = 1000, int rounds = 1, bool marked = true) {
    ScenarioBattle b(make_party(pet(marked && owner == 0, args)),
                     make_party(pet(marked && owner == 1, args)));
    b.ctx().is_boss_challenge = boss;
    b.start();
    auto& ctx = b.ctx();
    ctx.getPet(owner).hp = hp;
    for (int slot = 0; slot < 6; ++slot) ctx.ability_levels[owner][slot] = levels[slot];
    for (int r = 0; r < rounds; ++r) {
        const int hp_start = ctx.getPet(owner).hp;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_until(State::BATTLE_ROUND_END);
        CHECK_EQ(ctx.getPet(owner).hp, hp_start, "不能提前到ROUND_START回血");
        std::array<Levels, 2> before{};
        for (int side = 0; side < 2; ++side)
            for (int slot = 0; slot < 6; ++slot)
                before[side][slot] = ctx.ability_levels[side][slot];
        const int enemy_hp = ctx.getPet(1 - owner).hp;
        b.step_past(State::BATTLE_ROUND_END);
        CHECK_EQ(ctx.getPet(owner).hp, std::min(kMaxHp, hp_start + expected_gain),
                 "1877回合末回血量，满血封顶合法");
        CHECK_EQ(ctx.getPet(1 - owner).hp, enemy_hp, "只恢复印主体力");
        for (int side = 0; side < 2; ++side)
            for (int slot = 0; slot < 6; ++slot)
                CHECK_EQ(ctx.ability_levels[side][slot], before[side][slot],
                         "六个等级槽逐槽快照，含起始满cap槽");
        std::cout << label << " R" << r + 1 << " hp=" << hp_start << "->"
                  << ctx.getPet(owner).hp << '\n';
        b.drive();
    }
}
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    run_case("defense", {0,0,1,0,0,0}, {2,0}, 3000, false, 0, 1000, 2);
    run_case("special-defense-owner1-boss", {0,0,0,1,0,0}, {2,0}, 3000, true, 1);
    run_case("both-defenses", {0,0,1,1,0,0}, {2,0}, 3000);
    run_case("special-attack-is-not-defense", {0,1,0,0,0,0}, {2,0}, 0);
    run_case("other-slots-only", {1,0,0,0,1,1}, {2,0}, 0);
    run_case("no-boost", {0,0,0,0,0,0}, {2,0}, 0);
    run_case("negative-defense", {0,0,-1,-1,0,0}, {2,0}, 0);
    run_case("raw-db-denominator4", {0,0,1,0,0,0}, {4,0}, 1500);
    run_case("explicit-zero", {0,0,1,0,0,0}, {0,0}, 0);
    run_case("missing-args-fallback", {0,0,1,0,0,0}, {}, 3000);
    run_case("full-hp-and-level-cap", {6,6,6,6,6,6}, {2,0}, 3000, false, 0, kMaxHp);
    run_case("no-mark", {0,0,1,0,0,0}, {2,0}, 0, false, 0, 1000, 1, false);
    std::cout << "scenario_5230 PASS (K350 1877 双防任一提升回合末回血)\n";
    return 0;
}
