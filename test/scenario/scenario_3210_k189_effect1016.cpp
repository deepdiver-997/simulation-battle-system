#include <utils/sbs_rand.h>
// 场景 3210 —— K189 effect 1016（伤害阈值双分支族）：大乔 [280,100,280,1,1]/[280,80,…]
//
//   A 高伤分支（row2 100%）：atk 5000 红伤 >280 → 对手冰封（Icebound 15）；
//     且不置下回合旗（must_crit/preemptive_level 不动）。
//   B 低伤分支：atk 1 红伤 2 <280 → 下回合 FMR：preemptive_level+1 + must_crit
//     （r2 双 NONE 不消费旗标，回合末可读——2872 227 同款字段断言）。
//   C 高伤分支 80% 档（row1）：40 回合统计带（查后 dispel 独立观测，期望 ~32）。
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

SoulMark make_mark(int ice_pct) {
    SoulMark sm(1016, "K1016", "test", EffectArgs({280, ice_pct, 280, 1, 1}));
    sm.monster_id = 3418;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 高伤分支（row2 100%）：冰封落地 + 不置旗 ═════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "大乔", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(100));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.has_active_abnormal_status(1, kIcebound), true,
                 "A 高伤>280：100% 冰封落地");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "A 高伤分支不置先制旗");
        CHECK_EQ(ctx.ws.must_crit[0], false, "A 高伤分支不置必爆");
    }

    // ══ B 低伤分支：下回合先制+1 且必爆 ═══════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "大乔", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/1, /*hp=*/1000000,
                                           make_mark(100));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // r1 低伤攻击
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.has_active_abnormal_status(1, kIcebound), false,
                 "B 低伤<280：不冰封");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // r2 NONE 不消费旗
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "B 下回合所有技能先制+1");
        CHECK_EQ(ctx.ws.must_crit[0], true, "B 下回合攻击必爆旗");
    }

    // ══ C 80% 档（row1）：40 回合统计带 ═══════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "大乔", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(80));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);   // 攻击槽轮换
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kIcebound)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);   // 查后清场独立观测（K186 同款）
            }
        }
        std::cout << "  [C] 40 回合冰封落地 " << wins << "（期望 ~32）" << std::endl;
        CHECK(wins >= 18 && wins <= 38, "C 80% 档 40 回合统计带 18~38");
    }

    std::cout << "scenario_3210 PASS (K189 1016 伤害阈值双分支)" << std::endl;
    return 0;
}
