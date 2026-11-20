// 场景 1960 —— J103 对手提升先制威力族（1 宏，moves_lib/j_foeboost_prio_power.cpp）
//（2026-10-03，worktree ../agents/j103；号段 1960-1969 → T1960 两块）
//
// 断言口径：
//   - 载体驭云赶月 36704 [539]|[]：物130 prio0 纯链（啸天破/夺心箭 priority=2 会
//     污染先制断言，弃用记档）；
//   - 正例 = 对手预挂 +2 攻（stat_change 直授）→ 先制+1（慢速自身 R1 先手）AND
//     威力 ×2（物130·atk200 基带 [187,220] → ×2 [374,440]）双断言；
//   - 负例 = 对手干净 → 对手先手 + 基带。
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
constexpr int kYuYun = 36704;   // 驭云赶月[539]|[]：物130 prio0（对手有提升→先制+1且威力×2）

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

    // ══ T1960 539 对手有提升 → 先制+1 且威力×2（驭云赶月）══
    {
        // a) 正例：对手 +2 → 慢速自身先手 且 承伤 ×2
        {
            ScenarioBattle b(make_party(self_pet({kYuYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS,
                  "T1960.a 前置：对手 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1960.a 539：先制+1 → 慢速自身先手");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 374 && foe_loss <= 440,
                  "T1960.a 539：威力 ×2（实测 " + std::to_string(foe_loss) +
                      "，带 [374,440]）");
        }
        // b) 负例：对手干净 → 对手先手 + 基带
        {
            ScenarioBattle b(make_party(self_pet({kYuYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1960.b 539 门：对手无提升 → 对手先手");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 187 && foe_loss <= 220,
                  "T1960.b 539 门：无翻倍基带（实测 " + std::to_string(foe_loss) +
                      "，带 [187,220]）");
        }
        std::cout << "  [T1960] 539 对手提升先制威力 通过\n";
    }

    std::cout << "scenario_1960 PASS (J103 对手提升先制威力族 539)" << std::endl;
    return 0;
}
