#include <utils/sbs_rand.h>
// 场景 2941 —— K14 effect 2441 超级行（卡布达超级形态 4829 [30,50,3,10,1]）
//
// 断言口径：
//   A 登场分段麻痹免疫：满体 → 3 段 → 麻痹施加被拒。
//   B 攻伤窗 50 + 双满足 → 回合末回 1/3（300000→633333，双方 NONE 无噪声）。
//   C 吸取攻击技限定：属性技不掉对手血；双满足回血照常（+1/3），恰证"无吸取份额"。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kGaoSu = 20017;   // 属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(int skill0) {
    ElfPet p = scenario::make_pet(1, "持有方", {skill0, 10005, 10005, 10005, 10005},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                  make_mark(2441, {30, 50, 3, 10, 1}, 4829));
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

    // A 登场分段麻痹免疫（满体 3 段）
    {
        ScenarioBattle b(scenario::make_party(holder_pet(kPaiDa)),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        apply_anomaly(&ctx, 0, /*麻痹=*/0, 3, 1);
        CHECK(!ctx.has_active_abnormal_status(0, 0), "A 满体 3 段 → 麻痹免疫（施加被拒）");
    }

    // B 攻伤窗 50 + 双满足回合末回 1/3
    {
        ScenarioBattle b(scenario::make_party(holder_pet(kPaiDa)),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 300000;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 50, "B1 超级行攻伤窗 50");
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 300000 + 1000000 / 3,
                 "B2 双满足 → 回合末回最大体力 1/3");
    }

    // C 吸取攻击技限定
    {
        ScenarioBattle b(scenario::make_party(holder_pet(kGaoSu)),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 500;
        const int dummy_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(dummy_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 0,
                 "C1 属性技不触发吸取（也无伤害）");
        // 双满足（对手无提升+无回合类）→ 回合末回 1/3；无吸取份额 → 恰好 1/3
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 500 + 1000000 / 3,
                 "C2 属性技无吸取回血（只有双满足 1/3）");
    }

    std::cout << "scenario_2941 PASS (K14 effect2441 超级行)" << std::endl;
    return 0;
}
