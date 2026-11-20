#include <utils/sbs_rand.h>
// 场景 3370 —— K201 effect 334（速度提升回合末固伤族）：炽凰·朱雀 2676（零参）
//
//   A 速度提升（+2）→ r1 末对手掉 max/6 = 166666（精确）。
//   B 未提升 → 零削。
//   C 速度负态（-1）→ 零削（门判 >0）。
//   D 提升持续两回合 → 累计 2×166666。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技
constexpr int kSpeedSlot = 4;

SoulMark make_mark() {
    SoulMark sm(334, "K334", "test", EffectArgs());
    sm.monster_id = 2676;
    return sm;
}

// 带 mark 跑 rounds 回合（双方 NONE，speed_boost=开局给 0 方速度槽写的等级），
// 返回对手总掉血。
int probe_foe_loss(int speed_boost, int rounds) {
    ElfPet holder = scenario::make_pet(1, "炽凰·朱雀", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                       make_mark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.ability_levels[0][kSpeedSlot] = speed_boost;
    const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    constexpr int kSixth = 1000000 / 6;   // 166666（1/6 整除取整）

    // ══ A 速度 +2：r1 末削 1/6 ════════════════════════════════════════════
    CHECK_EQ(probe_foe_loss(2, 1), kSixth, "A 速度提升 → 对手掉 max/6");

    // ══ B 未提升：零削 ════════════════════════════════════════════════════
    CHECK_EQ(probe_foe_loss(0, 1), 0, "B 未提升：零削");

    // ══ C 速度负态：零削（门判 >0）════════════════════════════════════════
    CHECK_EQ(probe_foe_loss(-1, 1), 0, "C 速度负态：零削");

    // ══ D 两回合累计 2/6 ══════════════════════════════════════════════════
    CHECK_EQ(probe_foe_loss(2, 2), 2 * kSixth, "D 提升持续：累计 2×max/6");

    std::cout << "scenario_3370 PASS (K201 334 速度提升回合末固伤)" << std::endl;
    return 0;
}
