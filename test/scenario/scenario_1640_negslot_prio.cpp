// 场景 1640 —— J74 负槽门先制族（1 宏，moves_lib/j_negslot_prio.cpp）
//（2026-10-03，worktree ../agents/j74；号段 1640-1649 → T1640 两块）
//
// 断言口径：
//   - 747 选择期生效：R1 当次先制判定即吃加成（selection_effects_ 表路由——
//     J66 1491 勘误正位）；自身预挂 -1 攻（stat_change 负 delta，不走免弱）；
//   - 载体千古幻灭决 18385 [747]|[1]（特150 prio 0 纯链）；断言只看 preemptive_right，
//     红伤带 [216,254] 不进断言面；
//   - 负例 = 自身无负槽 → 无加成 → 对手（speed 300）先手。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kQianGu = 18385;   // 千古幻灭决[747]|[1]：特150（自身有下降→先制+1）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(1, "己方", skills, speed, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(2, "对手", skills, speed, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1640 747 自身有下降 → 先制+1（千古幻灭决 [1]）══
    {
        // a) 正例：自身 -1 攻 → R1 慢速自身先手
        {
            ScenarioBattle b(make_party(self_pet({kQianGu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS,
                  "T1640.a 前置：自身 -1 攻（有负槽）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1640.a 747：负槽先制+1 → 慢速自身 R1 先手");
        }
        // b) 负例：自身无负槽 → 对手先手
        {
            ScenarioBattle b(make_party(self_pet({kQianGu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1640.b 747 门：无下降 → 对手（speed 300）先手");
        }
        std::cout << "  [T1640] 747 负槽门先制 通过\n";
    }

    std::cout << "scenario_1640 PASS (J74 负槽门先制族 747)" << std::endl;
    return 0;
}
