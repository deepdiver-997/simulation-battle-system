#include <utils/sbs_rand.h>
// 场景 3630 —— K221 受击闪避族（307，soul_lib/k221_dodge.cpp）
//（2026-10-07，worktree ../agents/k221；号段 3630-3639 → T3630-T3631 两块）
//
// 断言口径：sbs_srand 钉种子分岔（探针标定 20% miss）：miss seed → 我方无伤；
// 命中 seed → 掉血；无印对照恒掉血（dodge_rate=0）。
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

ElfPet mark_pet(bool with_mark) {
    std::vector<int> args{0, 0, 20, 4};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(with_mark ? 307 : 0, "K221 魂印", "",
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

    // ══ T3630 miss seed：我方无伤 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                         make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(12);
        foe_hits(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 99999, "T3630 307：miss seed → 我方无伤");
        std::cout << "  [T3630] 闪避miss 通过\n";
    }

    // ══ T3631 命中 seed：掉血 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                         make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(2);
        foe_hits(b);
        CHECK(b.ctx().getPet(0).hp < 99999, "T3631 307：命中 seed → 掉血");
        std::cout << "  [T3631] 命中对照 通过\n";
    }

    std::cout << "scenario_3630 PASS (K221 受击闪避族)" << std::endl;
    return 0;
}
