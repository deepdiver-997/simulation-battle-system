// 场景 1800 —— J91 对手负槽先制族（1 宏，moves_lib/j_foenslot_prio.cpp）
//（2026-10-03，worktree ../agents/j91；号段 1800-1809 → T1800 两块）
//
// 断言口径：
//   - 1243 选择期生效：对手预挂 -1 攻（stat_change 直写）→ 自身慢速仍 R1 先手；
//   - 载体飞燕无影剑 31122 [828,1243,40]|[] 全零参链：828=自身提升先制+1（自身无提升
//     自灭）、40=先手威力×2（只改威力不碰先制断言）——双良性；
//   - 负例 = 对手无负槽 → 对手（speed 300）先手。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kFeiYan = 31122;  // 飞燕无影剑[828,1243,40]|[]：物150 全零参链（对手有下降→先制+1）

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

    // ══ T1800 1243 对手有下降 → 先制+1（飞燕无影剑）══
    {
        // a) 正例：对手 -1 攻 → R1 慢速自身先手
        {
            ScenarioBattle b(make_party(self_pet({kFeiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, -1) == StatChangeResult::SUCCESS,
                  "T1800.a 前置：对手 -1 攻（有负槽）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1800.a 1243：对手负槽先制+1 → 慢速自身 R1 先手");
        }
        // b) 负例：对手无负槽 → 对手先手
        {
            ScenarioBattle b(make_party(self_pet({kFeiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1800.b 1243 门：对手无下降 → 对手（speed 300）先手");
        }
        std::cout << "  [T1800] 1243 对手负槽先制 通过\n";
    }

    std::cout << "scenario_1800 PASS (J91 对手负槽先制族 1243)" << std::endl;
    return 0;
}
