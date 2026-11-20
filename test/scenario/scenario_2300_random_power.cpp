// 场景 2300 —— J135 变威力族二（61/70，moves_lib/j_random_power.cpp）
//（2026-10-04，worktree ../agents/j135；号段 2300-2309）
//
// 断言口径（直断 ws.skill_power_view——scenario_009 同款，视图随对局保持到断言点）：
//   - 61 峰回路转（物，基线威力=载表面板）：10 次对局采样全部落 [50,150]；
//   - 70 远古审判（特）：10 次采样全部落 [150,220]；
//   - 随机性侧证：61 两批采样并集宽 > 20（band 101 宽，20 样本全同概率 ~1e-18 量级，
//     断言防"写死不掷骰"退化）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打（填充）
constexpr int kFengHui = 10484;   // 峰回路转 [61]（物）
constexpr int kYuanGu  = 10847;   // 远古审判 [70]（特）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "随机威力", skills, /*speed=*/300, /*atk=*/1, 5000);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2300 61 band [50,150] + 随机性侧证 ══
    {
        int lo = 9999, hi = -1;
        for (int t = 0; t < 10; ++t) {
            ScenarioBattle b(make_party(self_pet({kFengHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int v = ctx.ws.skill_power_view[0].value;
            CHECK(v >= 50 && v <= 150,
                  "T2300 61 采样越带（实测 " + std::to_string(v) + "）");
            lo = lo < v ? lo : v;
            hi = hi > v ? hi : v;
        }
        CHECK(hi - lo > 20, "T2300 61 随机性侧证：样本极差>20（" + std::to_string(lo) +
                                "~" + std::to_string(hi) + "）");
        std::cout << "  [T2300] 61 威力随机 [50,150] 通过\n";
    }

    // ══ T2301 70 band [150,220] ══
    {
        int lo = 9999, hi = -1;
        for (int t = 0; t < 10; ++t) {
            ScenarioBattle b(make_party(self_pet({kYuanGu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int v = ctx.ws.skill_power_view[0].value;
            CHECK(v >= 150 && v <= 220,
                  "T2301 70 采样越带（实测 " + std::to_string(v) + "）");
            lo = lo < v ? lo : v;
            hi = hi > v ? hi : v;
        }
        CHECK(hi - lo > 10, "T2301 70 随机性侧证：样本极差>10（" + std::to_string(lo) +
                                "~" + std::to_string(hi) + "）");
        std::cout << "  [T2301] 70 威力随机 [150,220] 通过\n";
    }

    std::cout << "scenario_2300 ALL PASS" << std::endl;
    return 0;
}
