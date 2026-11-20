#include <utils/sbs_rand.h>
// 场景 2902 —— K11 effect 324（被击败→对手降 1 体+清强化）+ 392（回合末低体对齐）
//
// 断言口径：
//   A 324：持有方 10 体被一击必杀 → 对手 hp==1（复仇）。
//   B 392：持有方 10% 体（<1/2 且 <对手）→ ROUND_END 对手体力对齐到持有方。
//   C 392 负例：持有方过半血 → 不对齐。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(int effect_id, int hp) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/hp,
                              make_mark(effect_id, effect_id == 324 ? std::vector<int>{1, 0}
                                                                    : std::vector<int>{2, 0},
                                        effect_id == 324 ? 2648 : 2818));
}
ElfPet attacker_pet() {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/1000000);
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

    // A 324 复仇
    {
        ScenarioBattle b(scenario::make_party(holder_pet(324, 10)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        (void)stat_change(&ctx, 1, 0, 2);   // 对手有强化 → 应被清
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 1, "A 被击败 → 对手降为 1 体");
        CHECK_EQ(ctx.ws.view_levels[1][0], 0, "A2 对手强化被清");
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 0, "A3 持有方已倒");
    }

    // B/C 392 对齐
    {
        ScenarioBattle b(scenario::make_party(holder_pet(392, 1000000)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 100000;   // 10% 体
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 500000;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 100000,
                 "B 低体（10%<50% 且 <对手）→ 对手体力对齐");
    }
    {
        ScenarioBattle b(scenario::make_party(holder_pet(392, 800000)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 900000;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 900000, "C 过半血 → 不对齐");
    }

    std::cout << "scenario_2902 PASS (K11 effect324/392)" << std::endl;
    return 0;
}
