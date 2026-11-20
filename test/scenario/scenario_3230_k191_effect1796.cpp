#include <utils/sbs_rand.h>
// 场景 3230 —— K191 effect 1796（强化冰封/降态反转固伤族）：卞玉儿 [50,300,1]/[60,400,1]
//
//   A 反转+固伤：自身 -2 攻 → r1 末反转为 +2 且对手掉 300 血（row1 精确差账）。
//   B row2 固伤 400：同布局 → 掉 400（args 驱动）。
//   C 无降态：不反转不附加（等级不动、对手零掉血）。
//   D 强化冰封：自身 +2 → 40 回合攻击统计带（查后 dispel 独立观测，期望 ~20）。
//   E 无强化：40 回合零冰封。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);
constexpr int kAtkSlot = 0;      // 等级槽 0=攻击

SoulMark make_mark(int ice_pct, int fixed) {
    SoulMark sm(1796, "K1796", "test", EffectArgs({ice_pct, fixed, 1}));
    sm.monster_id = 4203;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 降态反转+固伤 300（row1）════════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "卞玉儿", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(50, 300));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.ability_levels[0][kAtkSlot] = -2;
        const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "A 降态反转为提升（-2→+2）");
        CHECK_EQ(foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 300,
                 "A 附加 300 固伤打对手");
    }

    // ══ B row2 固伤 400（args 驱动）═══════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "卞玉儿", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(60, 400));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.ability_levels[0][kAtkSlot] = -1;
        const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 400,
                 "B row2 附加 400 固伤（args 驱动）");
    }

    // ══ C 无降态：不反转不附加 ════════════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "卞玉儿", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(50, 300));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 0, "C 无降态等级不动");
        CHECK_EQ(foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 0, "C 无降态零固伤");
    }

    // ══ D 强化冰封 50%：40 回合统计带 ═════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "卞玉儿", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(50, 300));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.ability_levels[0][kAtkSlot] = 2;   // 回合开始评强化（ROUND_START 在首驱跑）
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kIcebound)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);
            }
        }
        std::cout << "  [D] 40 回合冰封落地 " << wins << "（期望 ~20）" << std::endl;
        CHECK(wins >= 8 && wins <= 32, "D 50% 档 40 回合统计带 8~32");
    }

    // ══ E 无强化：40 回合零冰封 ═══════════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "卞玉儿", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(50, 300));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        bool ever = false;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kIcebound)) {
                ever = true;
            }
        }
        CHECK_EQ(ever, false, "E 无强化：零冰封");
    }

    std::cout << "scenario_3230 PASS (K191 1796 强化冰封/降态反转固伤)" << std::endl;
    return 0;
}
