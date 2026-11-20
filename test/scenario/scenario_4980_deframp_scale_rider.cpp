#include <utils/sbs_rand.h>
// 场景 4980 —— K342 双防递增+提升段数附加族（707，soul_lib/k342_deframp_scale_rider.cpp）
//（2026-10-21，worktree ../agents/k342；号段 4980-4989 → T4980 一块四断言）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；对手 NONE；逐回合双防 +1 递增）：
//   探针实证轮序：start() 不触发 ROUND_START，首个 idle_round 驱动 R1（双防 1,1）；
//   攻击轮 R2 的 ROUND_START 再 +1（2,2）→ 段数 4 → 附加 200 → 掉血 [287,302]。
//   再 idle R3（3,3）→ 攻击轮 R4 段数 6 → 附加 300 → 掉血 [387,402]。
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
    // DB args 原序 [1,50]：db[0]=1 双防增量、db[1]=50 段数乘数。
    SoulMark sm(707, "K342 魂印", "test", EffectArgs(std::vector<int>{1, 50}));
    sm.monster_id = 3007;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "海拉瑞德", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int mark_attack_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
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

    sbs_srand(4980);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    idle_round(b);   // R1：ROUND_START 双防 +1（1,1）
    CHECK_EQ(ctx.ability_levels[0][2], 1,
             "T4980 707：R1 防御 +1（实测 lv=" +
                 std::to_string(ctx.ability_levels[0][2]) + "）");
    int drop = mark_attack_round(b);   // R2：ROUND_START 再 +1（2,2）→ 附加 (2+2)×50
    CHECK(drop >= 287 && drop <= 302,
          "T4980 707：R2 段数 (2+2)×50=200 → 掉血 [287,302]（实测 " +
              std::to_string(drop) + "）");
    idle_round(b);   // R3：双防 (3,3)
    drop = mark_attack_round(b);   // R4：ROUND_START (4,4) → 附加 (4+4)×50=300
    CHECK(drop >= 487 && drop <= 502,
          "T4980 707：R4 段数 (4+4)×50=400 → 掉血 [487,502]（实测 " +
              std::to_string(drop) + "）");

    std::cout << "scenario_4980 PASS (K342 双防递增+提升段数附加族)" << std::endl;
    return 0;
}
