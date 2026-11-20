#include <utils/sbs_rand.h>
// 场景 3590 —— K216 effect 628（概率麻痹+伤害回血族）：雷伊 [25,0,1,25]/[35,0,1,25] +
// 极天斗皇 [30,0,1]
//
//   A 回血差账（雷伊 [25,…,25]）：对手掉血 X → 自身回 X×25%（同一浮动伤害推导）。
//   B args 驱动（[35,…,25]）：回血仍 25%（db[3] 不变）。
//   C 无回血子句（极天斗皇 无 db[3]）：零回血。
//   D 麻痹「所有技能」含属性技：25% 档 40 回合（属性技出手）统计带 3~17。
//   E 麻痹稀疏对照（25% 攻击档）已在 D 覆盖通道；无麻痹关面由 K176 通道验证承担。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

SoulMark make_mark(const std::vector<int>& db) {
    SoulMark sm(628, "K628", "test", EffectArgs(db));
    sm.monster_id = 2887;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 回血差账 25%（雷伊 [25,0,1,25]）═════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "雷神·雷伊", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark({25, 0, 1, 25}));
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
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        const int gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - my0;
        CHECK(loss > 1000, "A 基线量级合理");
        CHECK_EQ(gain, loss * 25 / 100, "A 回血 = 伤害×25%");
    }

    // ══ B args 驱动（[35,…,25]）：回血仍 25% ══════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "雷神·雷伊", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark({35, 0, 1, 25}));
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
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        const int gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - my0;
        CHECK_EQ(gain, loss * 25 / 100, "B 进化行：麻痹 35% 但回血仍 25%（db[3]）");
    }

    // ══ C 无回血子句（极天斗皇 [30,0,1]）：零回血 ═════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "极天斗皇", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark({30, 0, 1}));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 50000;
        const int my0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - my0, 0,
                 "C 无 db[3]：零回血");
    }

    // ══ D 「所有技能」含属性技：麻痹 25% 档 40 回合统计带 ══════════════════
    {
        ElfPet holder = scenario::make_pet(1, "雷神·雷伊", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark({25, 0, 1, 25}));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);   // 属性技
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kParalysis)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);
            }
        }
        std::cout << "  [D] 40 回合属性技麻痹落地 " << wins << "（期望 ~10）" << std::endl;
        CHECK(wins >= 3 && wins <= 17, "D 25% 档（含属性技）统计带 3~17");
    }

    std::cout << "scenario_3590 PASS (K216 628 概率麻痹+伤害回血)" << std::endl;
    return 0;
}
