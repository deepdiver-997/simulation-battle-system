#include <utils/sbs_rand.h>
// 场景 2901 —— K11 effect 304：受致死攻击概率重生（清双方升降）
//
// 断言口径：100% 行 [100,1] 确定性档。
//   A 致死一击 → 重生回满（args[1]=次数 1）。
//   B 重生时清双方提升与下降（预置 +2/-2 等级 → 重生后归零）。
//   C 消耗后再被击杀 → 真死（hp 0，无第二次重生）。
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
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(304, {100, 1}, 2607));
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

    ScenarioBattle b(scenario::make_party(holder_pet()),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();

    // 预置升降：持有方 +2（提升）、对手 -2（下降）→ 重生时应被清
    (void)stat_change(&ctx, 0, 0, 2);
    (void)stat_change(&ctx, 1, 0, -2);
    CHECK_EQ(ctx.ws.view_levels[0][0], 2, "前置：持有方攻击 +2");
    // 压到 10 体（上限保持 1M → 重生回满可断言；不压则打不死、断言空洞）
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 10;

    // A/B 致死一击 → 重生回满 + 清双方升降
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    CHECK(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp > 900000,
          "A 致死一击 → 重生回满（max=1000000）");
    // ⚠️ 读本体 ability_levels：ws.view_levels 是回合视图，清级后未必即时回同步。
    CHECK_EQ(ctx.ability_levels[0][0], 0, "B1 重生清自身提升（本体）");
    CHECK_EQ(ctx.ability_levels[1][0], 0, "B2 重生清对手下降（本体）");

    // C 消耗后再杀 → 真死（复活回满后再次压到 10 体，否则打不死）
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 10;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 0, "C 次数耗尽 → 再杀真死");

    std::cout << "scenario_2901 PASS (K11 effect304 致死重生+清升降)" << std::endl;
    return 0;
}
