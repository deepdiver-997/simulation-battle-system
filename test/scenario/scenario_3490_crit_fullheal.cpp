// 场景 3490 —— K208 暴击回满族（322，soul_lib/k208_crit_fullheal.cpp）
//（2026-10-07，worktree ../agents/k208；号段 3490-3499 → T3490-T3491 两块）
//
// 断言口径（SOP §五.2：先压血防空洞）：必暴档 → 打完即满血（99999）；
// 非暴击档（压血 5000）→ 不回（自身无来伤、无回血）。
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

ElfPet mark_pet(bool must_crit) {
    std::vector<int> args{0, 0, 100, 100};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/60, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(322, "K208 魂印", "",
                                               EffectArgs(std::move(args))));
    set_crit_sixteenths(p, must_crit ? 16 : 0);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3490 必暴：打完即回满 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*must_crit=*/true)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;   // 先压血（防空洞通过）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T3490 322：必暴 → 回满 99999");
        std::cout << "  [T3490] 必暴回满 通过\n";
    }

    // ══ T3491 非暴击：不回 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*must_crit=*/false)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, my0, "T3491 322：非暴击 → 不回血");
        std::cout << "  [T3491] 非暴击对照 通过\n";
    }

    std::cout << "scenario_3490 PASS (K208 暴击回满族)" << std::endl;
    return 0;
}
