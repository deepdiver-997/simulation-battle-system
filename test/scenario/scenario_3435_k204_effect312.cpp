#include <utils/sbs_rand.h>
// 场景 3435 —— K204 effect 312（强化吸血族）：混沌大天使 2626（args[1,100,30]）
//
//   A 强化攻击：+2 攻 → 吸血 = 对手掉血×30%（同一浮动伤害推导，差账精确）。
//   B 无强化：零吸血（holder hp 不动）。
//   C 负态（-2 攻）：零吸血（门判任一正槽）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kAtkSlot = 0;

SoulMark make_mark() {
    SoulMark sm(312, "K312", "test", EffectArgs({1, 100, 30}));
    sm.monster_id = 2626;
    return sm;
}

// 0 方带 mark（boost=攻槽等级）攻击一回合（1 方 NONE），返回 {对手掉血, 自身回血}。
std::pair<int, int> probe_lifesteal(int boost) {
    ElfPet holder = scenario::make_pet(1, "混沌大天使", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                       make_mark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.ability_levels[0][kAtkSlot] = boost;
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 50000;   // 压低避封顶
    const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    const int my0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    const int foe_loss = foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    const int my_gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - my0;
    return {foe_loss, my_gain};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 强化攻击：吸血 = 掉血×30% ═══════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(2);
        CHECK(loss > 1000, "A 基线量级合理");
        CHECK_EQ(gain, loss * 30 / 100, "A 吸血 = 伤害×30%（同一浮动伤害推导）");
    }

    // ══ B 无强化：零吸血 ══════════════════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(0);
        CHECK(loss > 1000, "B 攻击落地");
        CHECK_EQ(gain, 0, "B 无强化：零吸血");
    }

    // ══ C 负态：零吸血 ════════════════════════════════════════════════════
    {
        const auto [loss, gain] = probe_lifesteal(-2);
        CHECK(loss > 1000, "C 攻击落地");
        CHECK_EQ(gain, 0, "C 负态：零吸血");
    }

    std::cout << "scenario_3435 PASS (K204 312 强化吸血)" << std::endl;
    return 0;
}
