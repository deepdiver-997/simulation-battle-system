#include <utils/sbs_rand.h>
// 场景 4630 —— K307 先手后手分支增益族（843，soul_lib/k307_order_branch_boost.cpp）
//（2026-10-21，worktree ../agents/k307；号段 4630-4639 → T4630-T4631 两块）
//
// 断言口径（双方互拍；印主速 300=先手 / 100=后手两场）：
//   - T4630 先手场：回合末双防（槽2/3）各 +1，特攻/速度不动。
//   - T4631 后手场：回合末特攻（槽1）/速度（槽4）各 +1，双防不动。
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

SoulMark make_mark() {
    // DB args 原序 [1,0]：db[0]=1 增量（db[1] 废位）。
    SoulMark sm(843, "K307 魂印", "test", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 3222;
    return sm;
}

ElfPet mark_pet(int speed) {
    ElfPet p = make_pet(1, "炫羽", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, 800000, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, 800000);
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

    // ══ T4630 先手场：双防 +1 ══════════════════════════════════════════════════
    {
        sbs_srand(4630);
        ScenarioBattle b(make_party(mark_pet(/*speed=*/300)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        both_attack(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][2], 1,
                 "T4630 843：先手 → 防御 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][2]) + "）");
        CHECK_EQ(ctx.ability_levels[0][3], 1,
                 "T4630 843：先手 → 特防 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][3]) + "）");
        CHECK_EQ(ctx.ability_levels[0][1], 0,
                 "T4630 843：先手 → 特攻不动（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][1]) + "）");
        CHECK_EQ(ctx.ability_levels[0][4], 0,
                 "T4630 843：先手 → 速度不动（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][4]) + "）");
        std::cout << "  [T4630] 先手双防分支 通过\n";
    }

    // ══ T4631 后手场：特攻/速度 +1 ═════════════════════════════════════════════
    {
        sbs_srand(4631);
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),
                         make_party(foe_pet(/*speed=*/200)));
        b.start();
        both_attack(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][1], 1,
                 "T4631 843：后手 → 特攻 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][1]) + "）");
        CHECK_EQ(ctx.ability_levels[0][4], 1,
                 "T4631 843：后手 → 速度 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][4]) + "）");
        CHECK_EQ(ctx.ability_levels[0][2], 0,
                 "T4631 843：后手 → 防御不动（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][2]) + "）");
        CHECK_EQ(ctx.ability_levels[0][3], 0,
                 "T4631 843：后手 → 特防不动（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][3]) + "）");
        std::cout << "  [T4631] 后手特攻速度分支 通过\n";
    }

    std::cout << "scenario_4630 PASS (K307 先手后手分支增益族)" << std::endl;
    return 0;
}
