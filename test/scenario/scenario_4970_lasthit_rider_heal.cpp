#include <utils/sbs_rand.h>
// 场景 4970 —— K341 上轮受伤附加+回合末回血族（711，soul_lib/k341_lasthit_rider_heal.cpp）
//（2026-10-21，worktree ../agents/k341；号段 4970-4979 → T4970 一块四断言）
//
// 断言口径（双方互拍 atk=300：红伤 [87,102]；印主速 300 先手）：
//   R1：印主先手刀 → last=0 无附加（对手掉红伤 [87,102]）；对手反击 → 印主受伤
//   R1 ∈ [87,102]，回合末回血 200 → 净 +[98,113]。
//   R2：印主刀 → 附加 R1×50% ∈ [43,51] → 掉血 ∈ [130,153]。
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
    // DB args 原序 [50,1,200]：db[0]=50 附加%、db[2]=200 回血（db[1] 废位）。
    SoulMark sm(711, "K341 魂印", "test", EffectArgs(std::vector<int>{50, 1, 200}));
    sm.monster_id = 3014;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "神佑之翼·炽天使", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(4970);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = 100000;

    both_attack(b);   // R1：印主先手（last=0 无附加）；对手反击受伤
    const int foe_drop1 = 800000 - ctx.getPet(1).hp;
    CHECK(foe_drop1 >= 87 && foe_drop1 <= 102,
          "T4970 711：R1 印主刀无附加 → 纯红伤 [87,102]（实测 " +
              std::to_string(foe_drop1) + "）");
    const int mark_after_r1 = ctx.getPet(0).hp;
    CHECK(mark_after_r1 >= 100098 && mark_after_r1 <= 100113,
          "T4970 711：受伤 [87,102] - 回血 200 → 净 +[98,113]（实测 hp=" +
              std::to_string(mark_after_r1) + "）");

    const int foe0 = ctx.getPet(1).hp;
    both_attack(b);   // R2：印主刀附加 R1×50% = [43,51]
    const int foe_drop2 = foe0 - ctx.getPet(1).hp;
    CHECK(foe_drop2 >= 130 && foe_drop2 <= 153,
          "T4970 711：R2 印主刀 → 红伤+上轮 50% ∈ [130,153]（实测 " +
              std::to_string(foe_drop2) + "）");

    std::cout << "scenario_4970 PASS (K341 上轮受伤附加+回合末回血族)" << std::endl;
    return 0;
}
