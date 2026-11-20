#include <utils/sbs_rand.h>
// 场景 2852 —— K8 effect 203：攻伤提升 db[0]%（AMP）+ 受伤减 db[1]%（REDUCE_PCT）
//
// 断言口径（srand 对齐 rand 序 → 整数精确，940 同款；预期值用与实现相同的
// 整数链计算：AMP floor(base*(100+a)/100) → REDUCE floor(·(100-r)/100)）：
//   A 拉塞尔 [3,3]：受伤 -3%；且持有方攻击 +3%。
//   B 暗翼魔圣 [0,20]：只受伤 -20%，攻伤不加（db[0]=0 不挂 AMP）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet attacker_pet() {
    return scenario::make_pet(2, "进攻方", {kYanFan, 10005, 10005, 10005, 10005},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}
ElfPet holder_pet(std::vector<int> db_args, int atk, int hp) {
    return scenario::make_pet(1, "持有方", {kYanFan, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/atk, /*hp=*/hp,
                              make_mark(203, std::move(db_args), 2190));
}
ElfPet dummy_pet() {
    return scenario::make_pet(3, "木桩", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

// 持有方防守：0 方 NONE、1 方攻击 → 持有方损血。
int probe_defend_loss(std::vector<int> db_args) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args), 10, 1000000)),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

// 持有方进攻：1 方木桩 NONE、0 方攻击 → 木桩损血（持有方在 0 方、速度高先手）。
int probe_attack_gain(std::vector<int> db_args) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args), 5000, 100000)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    const int hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 拉塞尔 [3,3]：减伤 3% + 攻伤 +3%
    {
        const int base_def = probe_defend_loss({0, 0});   // db 全 0：两通道都不挂
        CHECK(base_def > 1000, "基线伤害量级合理");
        const int marked = probe_defend_loss({3, 3});
        CHECK_EQ(marked, base_def * 97 / 100, "A1 受伤减 3%（整数精确）");

        const int base_atk = probe_attack_gain({0, 0});
        CHECK(base_atk > 1000, "进攻基线量级合理");
        const int gained = probe_attack_gain({3, 3});
        CHECK_EQ(gained, base_atk * 103 / 100, "A2 攻伤 +3%（AMP 整数精确）");
    }

    // B 暗翼 [0,20]：只减伤
    {
        const int base_def = probe_defend_loss({0, 0});
        const int marked = probe_defend_loss({0, 20});
        CHECK_EQ(marked, base_def * 80 / 100, "B 受伤减 20%（db[0]=0 无 AMP）");
        const int gained = probe_attack_gain({0, 20});
        const int base_atk = probe_attack_gain({0, 0});
        CHECK_EQ(gained, base_atk, "B2 攻伤不加");
    }

    std::cout << "scenario_2852 PASS (K8 effect203 AMP+REDUCE_PCT 双通道)" << std::endl;
    return 0;
}
