#include <utils/sbs_rand.h>
// 场景 3380 —— K203 effect 344（暴击概率害怕族）：太古邪神 2709（args[30,6]）
//
//   A 必爆（ws.must_crit 直写）40 回合 → 害怕落地统计带（期望 ~12，带 4~20，
//     查后 dispel 独立观测）。
//   B 双方 NONE（无伤害无暴击）→ 零害怕（确定性关）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);

SoulMark make_mark() {
    SoulMark sm(344, "K344", "test", EffectArgs({30, 6}));
    sm.monster_id = 2709;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 必爆 40 回合：害怕落地带 4~20 ═══════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(
            1, "太古邪神", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
            /*speed=*/300, /*atk=*/5000, /*hp=*/1000000, make_mark());
        scenario::set_crit_sixteenths(holder, 16);   // 16/16 必暴（2891 同款；
                                                     // ws.must_crit 直写会被回合 ws 重置抹掉）
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kFear)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);
            }
        }
        std::cout << "  [A] 40 回合必爆害怕落地 " << wins << "（期望 ~12）" << std::endl;
        CHECK(wins >= 4 && wins <= 20, "A 30% 档 40 回合统计带 4~20");
    }

    // ══ B 无伤害回合：零害怕 ══════════════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "太古邪神", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark());
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        bool ever = false;
        for (int r = 0; r < 10; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kFear)) {
                ever = true;
            }
        }
        CHECK_EQ(ever, false, "B 无暴击无伤害：零害怕");
    }

    std::cout << "scenario_3380 PASS (K203 344 暴击概率害怕)" << std::endl;
    return 0;
}
