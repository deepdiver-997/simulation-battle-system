// 场景 3325 —— K199 受击消强族（180，soul_lib/k199_hit_clear_boost.cpp）
//（2026-10-07，worktree ../agents/k199；号段 3325-3334 → T3325-T3326 两块）
//
// 断言口径：攻击方带正槽打我 → 受击后攻方正槽清零（自身槽不动——只消"对手"）；
// 对手不动手 → 不清（受击门）。
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
    std::vector<int> args{0, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(180, "K199 魂印", "",
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3325 受击消强：攻方正槽清零，自身槽不动 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 攻击方带提升
        ctx.ability_levels[0][1] = 1;   // 自身（受击方）提升——不应被清
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3325 180：受击 → 攻击方提升被清");
        CHECK_EQ(ctx.ability_levels[0][1], 1, "T3325 180：自身提升不动（只消对手）");
        std::cout << "  [T3325] 受击消强 通过\n";
    }

    // ══ T3326 受击门：对手不动手 → 不清 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 2,
                 "T3326 180：对手未出手 → 提升保留（受击门）");
        std::cout << "  [T3326] 受击门对照 通过\n";
    }

    std::cout << "scenario_3325 PASS (K199 受击消强族)" << std::endl;
    return 0;
}
