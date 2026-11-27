#include <utils/sbs_rand.h>
// 场景 3530 —— K211 受击概率攻升族（315，soul_lib/k211_hit_atk_up.cpp）
//（2026-10-07，worktree ../agents/k211；号段 3530-3539 → T3530-T3531 两块）
//
// 断言口径：sbs_srand 钉种子分岔（探针标定 20% 掷点）：落地 seed → 攻槽 +1；
// 失手 seed → 攻槽不变。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;

ElfPet mark_pet() {
    std::vector<int> args{20, 1, 1};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(315, "K211 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_hits(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3530 落地 seed：攻 +1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(8);
        foe_hits(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 1, "T3530 315：落地 seed → 攻 +1");
        std::cout << "  [T3530] 受击攻升 通过\n";
    }

    // ══ T3531 失手 seed：攻不变 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(2);
        foe_hits(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T3531 315：失手 seed → 攻不变");
        std::cout << "  [T3531] 失手对照 通过\n";
    }

    std::cout << "scenario_3530 PASS (K211 受击概率攻升族)" << std::endl;
    return 0;
}
