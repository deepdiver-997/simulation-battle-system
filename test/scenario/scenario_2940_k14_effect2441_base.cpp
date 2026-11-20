#include <utils/sbs_rand.h>
// 场景 2940 —— K14 effect 2441 基础行（卡布达 4825 [0,40,0,0,1]）
//
// 断言口径：
//   A 攻伤窗门：对手有提升 → damage_add_pct 0；对手无提升 → 40。
//   B 先制：对手无回合类效果 → FIRST_MOVE_RIGHT 桶 +1（字段，debug_step 停泊读）。
//   C 吸取：用技后吸取对手最大体力 20%（1M→-200000，自身回 200000）。
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

ElfPet holder_pet() {
    ElfPet p = scenario::make_pet(1, "持有方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                  make_mark(2441, {0, 40, 0, 0, 1}, 4825));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A1 对手有提升 → 无攻伤窗
    {
        ScenarioBattle b(scenario::make_party(holder_pet()),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        (void)stat_change(&ctx, 1, 0, 2);   // 对手 +2 提升
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0, "A1 对手有提升 → 无攻伤窗");
    }
    // A2+B+C 对手干净 → 攻伤窗 40 + 先制+1 + 吸取
    {
        ScenarioBattle b(scenario::make_party(holder_pet()),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 500;   // 吸取回血可断言
        const int dummy_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "B 对手无回合类 → 先制+1（FIRST_MOVE_RIGHT 桶）");
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 40, "A2 无提升 → 攻伤窗 40");
        const int dummy_loss = dummy_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        const int holder_gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - 500;
        CHECK_EQ(holder_gain, 200000, "C 吸取 = 对手最大体力 20%（200000）");
        CHECK(dummy_loss > 200000, "C2 木桩掉血 ≥ 吸取量（红伤+吸取）");
    }

    std::cout << "scenario_2940 PASS (K14 effect2441 基础行)" << std::endl;
    return 0;
}
