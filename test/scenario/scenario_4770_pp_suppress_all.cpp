#include <utils/sbs_rand.h>
// 场景 4770 —— K321 全 PP 压制族（360，soul_lib/k321_pp_suppress_all.cpp）
//（2026-10-21，worktree ../agents/k321；号段 4770-4779 → T4770 一块两断言）
//
// 断言口径（对手 5 技全拍打 PP=35；印主 NONE 无扰动）：
//   - R1 末对手 PP 总量 175-5=170（每技能各 -1）。
//   - R3 末累计 175-15=160（逐回合叠加）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;

SoulMark make_mark() {
    // DB args 原序 [2,1]：db[1]=1 每技能降低量（db[0] 废位）。
    SoulMark sm(360, "K321 魂印", "test", EffectArgs(std::vector<int>{2, 1}));
    sm.monster_id = 2760;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "加百利", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, 800000, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int foe_pp_sum(BattleContext& ctx) {
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        sum += ctx.getPet(1).skills[i].pp;
    }
    return sum;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(4770);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    CHECK_EQ(foe_pp_sum(ctx), 175, "T4770 360：初始对手 PP 总量 175");
    idle_round(b);
    CHECK_EQ(foe_pp_sum(ctx), 170,
             "T4770 360：R1 → 全技能各 -1（总量 170，实测 " +
                 std::to_string(foe_pp_sum(ctx)) + "）");
    idle_round(b);
    idle_round(b);
    CHECK_EQ(foe_pp_sum(ctx), 160,
             "T4770 360：R3 → 累计各 -3（总量 160，实测 " +
                 std::to_string(foe_pp_sum(ctx)) + "）");

    std::cout << "scenario_4770 PASS (K321 全 PP 压制族)" << std::endl;
    return 0;
}
