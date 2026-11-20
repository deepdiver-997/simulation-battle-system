#include <utils/sbs_rand.h>
// 场景 300：910 当回合击败对手则令对手{0}回合内属性技能无效
// （2026-09-28，night-3 / D14 圣光格劳瑞 4004；实现 resources/moves_lib/d5_D14.cpp）
//
// 载体：王·风雷啸世 33994（真身技能，[760,942,967,910]/[3,35,2]；其余三句已有实现）。
// T1 击杀封属：对手 hp=1 → R1 击杀 → 替补吃 2 回合属性封属：R2 守方锋利（属性技）
//     被无效（攻+1 不落地）；R3 窗口过期 → 锋利照常落地。
// T2 未击败：R1 守方存活 → 不封属，R2 锋利照常落地。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;       // 拍打（物攻 40）
constexpr int kFengLei = 33994;   // 王·风雷啸世：[760,942,967,910] [3,35,2]
constexpr int kFengLi = 20265;    // 锋利：属性，自身攻击+1

ElfPet gelaorui() {
    ElfPet p = scenario::make_pet(4004, "圣光格劳瑞", {kFengLei, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kFengLi, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, int my_skill, int foe_skill) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 击杀 → 替补进入 2 回合属性封属 ==========
    {
        ScenarioBattle b(scenario::make_party(gelaorui()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        one_round(b, 0, 0);   // R1：击杀
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        one_round(b, 1, 0);   // R2：替补锋利（属性技）→ 被无效
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T1 R2 属性封属：锋利攻+1 未落地");
        one_round(b, 1, 0);   // R3：窗口过期，锋利照常
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T1 R3 窗口过期：锋利攻+1 落地");
        std::cout << "  [T1] 击杀 → 2 回合属性封属 通过\n";
    }

    //========== T2 未击败：不封属 ==========
    {
        ScenarioBattle b(scenario::make_party(gelaorui()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 1);   // R1：守方存活（守方用拍打）
        one_round(b, 1, 0);   // R2：锋利照常
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T2 未击败：锋利攻+1 照常落地");
        std::cout << "  [T2] 未击败 → 不封属 通过\n";
    }

    std::cout << "scenario_300 all passed" << std::endl;
    return 0;
}
