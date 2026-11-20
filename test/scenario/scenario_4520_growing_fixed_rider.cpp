#include <utils/sbs_rand.h>
// 场景 4520 —— K296 成长固伤 rider 族（629，soul_lib/k296_growing_fixed_rider.cpp）
//（2026-10-21，worktree ../agents/k296；号段 4520-4529 → T4520 一块四断言）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；对手 NONE；账本按"每回合"+50 封顶 300）：
//   单刀掉血 = 红伤 + 账本值 → 逐回合带宽断言（红伤浮动 ±8 远小于步长 50，带互不
//   相交）：R1 [130,160]、R3 [230,262]、R6 [385,407]、R7 同 R6（封顶）。
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
    // DB args 原序 [50,300]：db[0]=50 基数=步长、db[1]=300 上限。
    SoulMark sm(629, "K296 魂印", "test", EffectArgs(std::vector<int>{50, 300}));
    sm.monster_id = 2886;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "德尔波拉", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, 800000, make_mark());
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

int slap_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(4520);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    const int d1 = slap_round(b);   // R1：账本 50
    CHECK(d1 >= 130 && d1 <= 160,
          "T4520 629：R1 掉血 = 红伤+50 ∈ [130,160]（实测 " + std::to_string(d1) +
              "）");
    (void)slap_round(b);            // R2：账本 100（过程刀）
    const int d3 = slap_round(b);   // R3：账本 150
    CHECK(d3 >= 230 && d3 <= 262,
          "T4520 629：R3 掉血 = 红伤+150 ∈ [230,262]（实测 " + std::to_string(d3) +
              "）");
    for (int i = 0; i < 2; ++i) {
        (void)slap_round(b);        // R4/R5：账本 200/250（过程刀）
    }
    const int d6 = slap_round(b);   // R6：账本 300（恰达上限）
    CHECK(d6 >= 385 && d6 <= 407,
          "T4520 629：R6 掉血 = 红伤+300 ∈ [385,407]（实测 " + std::to_string(d6) +
              "）");
    const int d7 = slap_round(b);   // R7：账本封顶 300 不再增长
    CHECK(d7 >= 385 && d7 <= 407 && (d7 > d6 ? d7 - d6 : d6 - d7) <= 20,
          "T4520 629：R7 封顶不再增长（实测 " + std::to_string(d7) + "，与 R6 差 " +
              std::to_string(d7 > d6 ? d7 - d6 : d6 - d7) + "）");

    std::cout << "scenario_4520 PASS (K296 成长固伤 rider 族)" << std::endl;
    return 0;
}
