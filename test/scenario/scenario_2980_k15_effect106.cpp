#include <utils/sbs_rand.h>
// 场景 2980 —— K15 effect 106：每次受到伤害反弹对手 60 点固定伤
//
// 断言口径（srand 对齐；反弹走 watcher 交付，整回合量测兜住）：
//   A 红伤局：攻方损血 == 裸局攻方损血 + 60。
//   B 粉伤也触发（"每次受到伤害"）：直灌 fixed 粉 100 → 攻方 -60。
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

ElfPet holder_pet(bool with_mark) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              with_mark ? make_mark(106, {60, 0}, 2672) : SoulMark{});
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

int probe_attacker_loss(bool with_mark) {
    ScenarioBattle b(scenario::make_party(holder_pet(with_mark)),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    const int atk_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return atk_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 红伤反弹
    {
        const int base = probe_attacker_loss(false);
        CHECK_EQ(probe_attacker_loss(true), base + 60, "A 攻方被反弹 60（对齐精确）");
    }
    // B 粉伤也反弹
    {
        ScenarioBattle b(scenario::make_party(holder_pet(true)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        const int atk_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);   // 灌在回合中段
        b.drive();                                              // 驱动到交付/flush
        const int atk_loss = atk_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        CHECK_EQ(atk_loss, 60, "B 直灌粉伤也触发反弹（回合内灌、drive 冲交付）");
    }

    std::cout << "scenario_2980 PASS (K15 effect106 定额反弹)" << std::endl;
    return 0;
}
