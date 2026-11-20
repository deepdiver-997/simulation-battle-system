// 场景 3810 —— K237 周期吸取族（665，soul_lib/k237_periodic_drain.cpp）
//（2026-10-08，worktree ../agents/k237；号段 3810-3819 → T3810-T3811 两块）
//
// 断言口径（行 [2,3]：每 2 回合吸对手 1/3 maxHP）：
//   - T3810 周期节奏：R1（rc=0）吸取 33333（opp maxHP/3；双方 NONE，唯一变量）+
//     自身回复同量（压血防封顶）；R2（rc=1）不吸（hp 不动）；R3（rc=2）再吸；
//   - 回复按 heal_amount 封顶、粉伤全额（k225 同口径），本用例压血不断言封顶分支。
//   ⚠️ 纯 drive() 流（3720 记档）。
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
constexpr int kMaxHp = 99999;

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(665, "K237 魂印", "",
                                               EffectArgs(std::vector<int>{2, 3})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3810 周期节奏：R1 吸 / R2 不吸 / R3 吸 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 30000;   // 压血防封顶（缺口 69999 > 33333）
        const int foe_hp0 = ctx.getPet(1).hp;

        both_none(b);   // R1（rc=0）：吸取
        CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, kMaxHp / 3,
                 "T3810 665：R1 吸取对手 1/3 maxHP（双方 NONE 唯一变量）");
        CHECK_EQ(ctx.getPet(0).hp - 30000, kMaxHp / 3,
                 "T3810 665：自身回复同量");

        const int foe_after_r1 = ctx.getPet(1).hp;
        const int self_after_r1 = ctx.getPet(0).hp;
        both_none(b);   // R2（rc=1）：周期外
        CHECK_EQ(ctx.getPet(1).hp, foe_after_r1, "T3810 665：R2 不吸取（对手）");
        CHECK_EQ(ctx.getPet(0).hp, self_after_r1, "T3810 665：R2 不回复（自身）");

        both_none(b);   // R3（rc=2）：再吸
        CHECK_EQ(foe_after_r1 - ctx.getPet(1).hp, kMaxHp / 3,
                 "T3810 665：R3 再吸 1/3");
        CHECK_EQ(ctx.getPet(0).hp - self_after_r1, kMaxHp / 3,
                 "T3810 665：R3 再回复");
        std::cout << "  [T3810] 周期节奏 通过\n";
    }

    std::cout << "scenario_3810 PASS (K237 周期吸取族)" << std::endl;
    return 0;
}
