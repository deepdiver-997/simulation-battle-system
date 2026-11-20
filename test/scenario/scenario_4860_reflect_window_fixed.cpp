#include <utils/sbs_rand.h>
// 场景 4860 —— K330 受击反弹+窗口附加固伤族（674，soul_lib/k330_reflect_window_fixed.cpp）
//（2026-10-21，worktree ../agents/k330；号段 4860-4869 → T4860 一块四断言）
//
// 断言口径（对手拍打 atk=300 → 印主受红伤 [87,102]；印主拍打 atk=300；对手 NONE）：
//   R1 对手打印主：反弹 [29,34]（30%）+ 挂窗（到期轮 R1+2=R3）。
//   R2/R3 印主刀：掉血 = 红伤 + 150 ∈ [237,252]。
//   R4 印主刀：窗过期 → 只红伤 [87,102]。
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
    // DB args 原序 [30,1,2,150]：db[0]=30 反弹%、db[2]=2 窗长、db[3]=150 附加。
    SoulMark sm(674, "K330 魂印", "test", EffectArgs(std::vector<int>{30, 1, 2, 150}));
    sm.monster_id = 2950;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "安格瑞", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return foe0 - ctx.getPet(1).hp;   // 对手掉血 = 反弹量
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

    sbs_srand(4860);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    const int rebound = foe_attack_round(b);   // R1：受击 → 反弹 + 挂窗
    CHECK(rebound >= 29 && rebound <= 34,
          "T4860 674：R1 受红伤 [87,102] → 反弹 30% ∈ [29,34]（实测 " +
              std::to_string(rebound) + "）");
    const int d2 = mark_attack_round(b);   // R2：窗内第 1 刀
    CHECK(d2 >= 237 && d2 <= 252,
          "T4860 674：R2 窗内 → 红伤+150 ∈ [237,252]（实测 " + std::to_string(d2) +
              "）");
    const int d3 = mark_attack_round(b);   // R3：窗内第 2 刀（到期轮）
    CHECK(d3 >= 237 && d3 <= 252,
          "T4860 674：R3 仍在窗 → 红伤+150（实测 " + std::to_string(d3) + "）");
    const int d4 = mark_attack_round(b);   // R4：窗过期
    CHECK(d4 >= 87 && d4 <= 102,
          "T4860 674：R4 窗过期 → 只红伤 [87,102]（实测 " + std::to_string(d4) +
              "）");

    std::cout << "scenario_4860 PASS (K330 受击反弹+窗口附加固伤族)" << std::endl;
    return 0;
}
