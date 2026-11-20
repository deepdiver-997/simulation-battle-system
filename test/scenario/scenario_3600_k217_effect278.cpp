#include <utils/sbs_rand.h>
// 场景 3600 —— K217 effect 278（登场窗吸血族）：诺伊尔顿 2509（零参）
//
//   A 窗内 r1 攻击：吸血 = 掉血×30%（同一浮动伤害推导，差账精确）。
//   B 窗外 r4 攻击：零吸血。
//   C 窗界 r3 攻击：仍吸血（含登场回合共 3 回合窗）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark() {
    SoulMark sm(278, "K278", "test", EffectArgs());
    sm.monster_id = 2509;
    return sm;
}

// 0 方带 mark 在第 attack_round 回合攻击（其余 NONE），返回 {对手掉血, 自身回血}。
std::pair<int, int> probe_lifesteal(int attack_round, int total_rounds) {
    ElfPet holder = scenario::make_pet(1, "诺伊尔顿", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                       make_mark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 50000;
    const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    const int my0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    for (int r = 1; r <= total_rounds; ++r) {
        b.submit_operation(0, r == attack_round ? BattleFsm::ActionType::SELECT_SKILL
                                                : BattleFsm::ActionType::NONE,
                           r == attack_round ? 0 : -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    const int loss = foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    const int gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - my0;
    return {loss, gain};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 窗内 r1 攻击：吸血 30% ══════════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(1, 1);
        CHECK(loss > 1000, "A 基线量级合理");
        CHECK_EQ(gain, loss * 30 / 100, "A 吸血 = 伤害×30%");
    }

    // ══ B 窗外 r4 攻击：零吸血 ════════════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(4, 4);
        CHECK(loss > 1000, "B 攻击落地");
        CHECK_EQ(gain, 0, "B 窗外（r4）：零吸血");
    }

    // ══ C 窗界 r3 攻击：仍吸血 ════════════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(3, 3);
        CHECK(loss > 1000, "C 攻击落地");
        CHECK_EQ(gain, loss * 30 / 100, "C r3（窗界）仍吸血");
    }

    std::cout << "scenario_3600 PASS (K217 278 登场窗吸血)" << std::endl;
    return 0;
}
