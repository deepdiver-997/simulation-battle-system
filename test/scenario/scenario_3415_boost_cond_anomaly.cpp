#include <utils/sbs_rand.h>
// 场景 3415 —— J182 提升态概率异常族（1227，moves_lib/j_boost_cond_anomaly.cpp）
//（2026-10-07，worktree ../agents/j182，看板段 3415-3424 → T3415-T3417）
//
// 载体：33881 淬锻焚灭焰（1227=[40,8]=40%睡眠；链上 1381 消回合反弹窗无回合类可消、
// 669 击败先制不触发——均无扰）。
//
// 断言口径（概率档运行期统计，K12 2920 口径；每回合开局治睡眠防跨回合残留双计）：
//   - T3415 无提升：不掷不施加 → 20 回合 0 睡眠（确定性）。
//   - T3416 有提升·后手（p=0.40）：20 回合睡眠次数 ∈ [3,13]（Binomial(20,0.4)
//     μ=8 σ≈2.19，±2.3σ）。
//   - T3417 有提升·先手（p=0.80 翻倍封顶前）：次数 ∈ [11,20]（μ=16 σ≈1.79，±2.8σ）；
//     与 T3416 带对比即"先手翻倍"的证据。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 33881;   // 淬锻焚灭焰（40% 睡眠）
constexpr int kSleep = 8;         // 官方异常码 8=睡眠

ElfPet holder_pet(int speed) {
    ElfPet p = make_pet(1, "印主", {kCarrier, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/100, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/1, /*hp=*/99999);
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

    const struct {
        int speed;        // 持印者速（300=先手 / 100=后手）
        bool boosted;     // 是否预置自身攻+2
        int expect_lo;
        int expect_hi;
        const char* tag;
    } cases[3] = {
        {300, false, 0, 0, "T3415 无提升"},
        {100, true, 3, 13, "T3416 有提升后手 p=0.4"},
        {300, true, 11, 20, "T3417 有提升先手 p=0.8"},
    };

    for (const auto& tc : cases) {
        ScenarioBattle b(make_party(holder_pet(tc.speed)),
                         make_party(plain_foe(/*speed=*/200)));
        sbs_srand(11);
        b.start();
        if (tc.boosted) {
            core_api().stat_change(&b.ctx(), /*target=*/0, /*stat=*/0, /*delta=*/2);
        }
        int sleeps = 0;
        for (int r = 0; r < 20; ++r) {
            // 治睡眠防跨回合残留双计（只清码 8）；载体 PP=5 → 每回合回满槽 0
            const int sleep_id = kSleep;
            (void)core_api().cure_anomalies(&b.ctx(), 1, &sleep_id, 1);
            (void)core_api().pp_restore_slot(&b.ctx(), 0, 0, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (b.ctx().has_active_abnormal_status(1, kSleep)) {
                ++sleeps;
            }
        }
        if (tc.expect_hi == 0) {
            CHECK_EQ(sleeps, 0, "T3415 1227：无提升 → 不施加（20 回合 0 睡眠）");
        } else {
            CHECK(sleeps >= tc.expect_lo && sleeps <= tc.expect_hi,
                  "T3416/T3417 1227：睡眠次数落统计带");
        }
        std::cout << "  [" << tc.tag << "] sleeps=" << sleeps << "/20\n";
    }

    std::cout << "scenario_3415 PASS (J182 提升态概率异常族)" << std::endl;
    return 0;
}
