#include <utils/sbs_rand.h>
// 场景 2872 —— K10 effect 227（每三回合必爆）+ 217（首回合致命锁 1 血）
//
// 断言口径：
//   A 227 字段断言：三回合 NONE 驱动，round3 末 ws.must_crit[0]==true（round1/2 false；
//     无攻击动作 → must_crit 不被消费、留到回合末可读）。
//   B 217 行为断言：持有方 10 体 vs atk5000 一击必杀 → hp==1（首回合锁血）。
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

ElfPet holder_pet_227() {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(227, {}, 1909));
}
ElfPet holder_pet_217(int hp) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/hp,
                              make_mark(217, {}, 1731));
}
ElfPet attacker_pet() {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
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

    // A 227：三轮 NONE，round3 必爆旗标
    {
        ScenarioBattle b(scenario::make_party(holder_pet_227()),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(20261004);
        b.start();
        for (int r = 1; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], r == 3,
                     "A round" + std::to_string(r) + " must_crit 旗标=" +
                         std::to_string(r == 3));
        }
    }

    // B 217：首回合致命伤锁 1 血
    {
        ScenarioBattle b(scenario::make_party(holder_pet_217(10)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(20261004);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 1,
                 "B 首回合致命伤 → 保留 1 点体力");
    }

    std::cout << "scenario_2872 PASS (K10 effect227/217)" << std::endl;
    return 0;
}
