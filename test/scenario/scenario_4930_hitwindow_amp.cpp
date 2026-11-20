#include <utils/sbs_rand.h>
// 场景 4930 —— K337 受击窗增伤族（695，soul_lib/k337_hitwindow_amp.cpp）
//（2026-10-21，worktree ../agents/k337；号段 4930-4939 → T4930 一块三断言）
//
// 断言口径（对手拍打 atk=300 → 印主受红伤 [87,102]；印主拍打 atk=300 红伤带
// [87,102]，窗内 ×2 → [174,204]；对手 NONE 除受击轮）：
//   R1 对手打印主：挂窗（到期轮 R1+2=R3）。
//   R2/R3 印主刀：掉血 ∈ [174,204]。R4：窗过期 → [87,102]。
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
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [1,2,100]：db[1]=2 窗长、db[2]=100 增伤%（db[0] 废位）。
    SoulMark sm(695, "K337 魂印", "test", EffectArgs(std::vector<int>{1, 2, 100}));
    sm.monster_id = 2990;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "斯洛斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return 0;
}

int mark_attack_round(ScenarioBattle& b) {
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

    sbs_srand(4930);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    foe_attack_round(b);   // R1：受击挂窗（到期轮 R3）
    const int d2 = mark_attack_round(b);   // R2：窗内 ×2
    CHECK(d2 >= 174 && d2 <= 204,
          "T4930 695：R2 窗内 → 红伤×2 ∈ [174,204]（实测 " + std::to_string(d2) +
              "）");
    const int d3 = mark_attack_round(b);   // R3：窗内第 2 刀（到期轮）
    CHECK(d3 >= 174 && d3 <= 204,
          "T4930 695：R3 仍在窗 → 红伤×2（实测 " + std::to_string(d3) + "）");
    const int d4 = mark_attack_round(b);   // R4：窗过期
    CHECK(d4 >= 87 && d4 <= 102,
          "T4930 695：R4 窗过期 → 只红伤 [87,102]（实测 " + std::to_string(d4) +
              "）");

    std::cout << "scenario_4930 PASS (K337 受击窗增伤族)" << std::endl;
    return 0;
}
