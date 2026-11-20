// 场景 3640 —— K222 对手等级变化回血族（338，soul_lib/k222_level_change_heal.cpp）
//（2026-10-07，worktree ../agents/k222；号段 3640-3649 → T3640-T3641 两块）
//
// 断言口径：印主用瞪眼（effect 5 降属技）压对手等级 → RC 回 max/6（99999/6=16666，
// 先压血 5000 防封顶）；无等级变化的攻击（撞击）→ 不回。
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

constexpr int kJi = 10001;     // 撞击

bool foe_dropped(BattleContext& ctx) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[1][i] < 0) {
            return true;
        }
    }
    return false;
}

ElfPet mark_pet() {
    std::vector<int> args{0, 0, 2, 6};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(338, "K222 魂印", "",
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

void my_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3640 等级变化 → RC 回 max/6 ══
    //（停泊 ROUND_START 后直改对手等级——确定性等价于"本回合发生了等级变化"）
    {
        ScenarioBattle b(make_party(mark_pet()),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        ctx.ability_levels[1][2] = -1;   // 对手防御 -1（回合内变化）
        b.drive();
        REQUIRE(foe_dropped(ctx));
        CHECK_EQ(ctx.getPet(0).hp, my0 + 99999 / 6,
                 "T3640 338：对手等级变化 → RC 回 1/6 最大 HP（16666）");
        std::cout << "  [T3640] 等级变化回血 通过\n";
    }

    // ══ T3641 无等级变化 → 不回 ══
    {
        ScenarioBattle b(make_party(mark_pet()),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        my_round(b);
        CHECK_EQ(ctx.getPet(0).hp, my0, "T3641 338：无等级变化 → 不回血");
        std::cout << "  [T3641] 无变化对照 通过\n";
    }

    std::cout << "scenario_3640 PASS (K222 对手等级变化回血族)" << std::endl;
    return 0;
}
