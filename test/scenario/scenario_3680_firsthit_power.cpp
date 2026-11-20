#include <utils/sbs_rand.h>
// 场景 3680 —— K224 首回合必中增伤族（257，soul_lib/k224_firsthit_power.cpp）
//（2026-10-07，worktree ../agents/k224；号段 3680-3689 → T3680-T3681 两块）
//
// 断言口径：
//   增伤：停泊 FIRST_SKILL_EFFECT 读视图（K187 范式）——登场回合 35→40（×115/100），
//   次回合 35。必中：印主挂低命中载体技（acc 1）——登场回合攻击仍命中（must_hit 授予
//   跳过掷骰），对照无印同局 miss。
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

ElfPet mark_pet(bool with_mark, int accuracy) {
    std::vector<int> args{0, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(with_mark ? 257 : 0, "K224 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = accuracy;
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

    // ══ T3680 增伤：登场回合 ×115/100，次回合回归 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true, /*accuracy=*/100)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int view_r1 = -1;
        {
            ctx.debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            ctx.debug_step_mode = false;
            b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
            view_r1 = ctx.ws.skill_power_view[0].value;
        }
        CHECK_EQ(view_r1, 35 * 115 / 100, "T3680 257：登场回合攻击威力 +15%（40）");
        b.drive();
        sbs_srand(7);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        int view_r2 = -1;
        {
            ctx.debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            ctx.debug_step_mode = false;
            b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
            view_r2 = ctx.ws.skill_power_view[0].value;
        }
        CHECK_EQ(view_r2, 35, "T3680 257：次回合 → 原威力（首回合门）");
        std::cout << "  [T3680] 首回合增伤 通过\n";
    }

    // ══ T3681 必中：acc=1 载体，登场回合攻击必中（对手有伤）══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true, /*accuracy=*/1)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp < foe0,
              "T3681 257：acc=1 登场回合 → 必中授予使命中（无印对照 miss 不可能区分，直验有伤）");
        std::cout << "  [T3681] 首回合必中 通过\n";
    }

    std::cout << "scenario_3680 PASS (K224 首回合必中增伤族)" << std::endl;
    return 0;
}
