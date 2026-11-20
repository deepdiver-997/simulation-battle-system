// 场景 3700 —— K225 零伤吸取族（341，soul_lib/k225_zerodmg_drain.cpp）
//（2026-10-07，worktree ../agents/k225；号段 3700-3709 → T3700-T3701 两块）
//
// 断言口径：无印主出伤的回合 → RC 吸取：opp 掉 max/3（99999/3=33333）、self +33333
//（先压血防封顶）；印主造成伤害的回合 → 不吸取。
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
    std::vector<int> args{0, 0, 30, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(341, "K225 魂印", "",
                                               EffectArgs(std::move(args))));
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

    // ══ T3700 零伤回合 → RC 吸取（opp -33333 / self +33333）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;   // 压血防封顶
        const int my0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 33333,
                 "T3700 341：零伤回合 → RC 吸取 opp 1/3 最大 HP");
        CHECK_EQ(ctx.getPet(0).hp - my0, 33333, "T3700 341：自身回同量");
        std::cout << "  [T3700] 零伤吸取 通过\n";
    }

    // ══ T3701 有伤回合 → 不吸取 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int foeLoss = foe0 - ctx.getPet(1).hp;
        CHECK(foeLoss > 0 && foeLoss < 33333,
              "T3701 341：有伤回合 → 不吸取（仅攻击红伤）");
        std::cout << "  [T3701] 有伤对照 通过\n";
    }

    std::cout << "scenario_3700 PASS (K225 零伤吸取族)" << std::endl;
    return 0;
}
