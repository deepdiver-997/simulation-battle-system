// 场景 1530 —— J66 反转先制族（2 宏，moves_lib/j_reversal_prio.cpp）
//（2026-10-03，worktree ../agents/j66；号段 1530-1539 → T1530-T1531 两块）
//
// 断言口径：
//   - 743 = 对手预挂 +2 攻（stat_change 直授）→ 反转后对手槽位翻 -2 + 下 1 回合先制+1
//     （慢速自身 R2 先手、R3 过期回对手）；负例 = 对手无提升（NOTHING）不武装窗；
//   - 1491 = 属性直改造天敌（我水 2 / 对手草 1，草克水，037/970 同款）：选择期先制+1
//     → 慢速自身 R1 先手；负例 = 同属性无天敌 → 对手先手；
//   - 载体链污染排查：17278 劈天狂击 [743] 单效果；37332 临风利爪 [1491,956]|[250]
//     （956=R2 用技后附加固伤，已注册 J12——T1531 只断 R1 先手，链尾不碰断言面）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物40（探针攻击）
constexpr int kPiTian = 17278;   // 劈天狂击[743]|[1,1]：物130（反转成功→下1回合先制+1）
constexpr int kLinFeng = 37332;  // 临风利爪[1491,956]|[250]：物150（遇天敌先制+1）

constexpr int kGrass = 1;
constexpr int kWater = 2;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

void set_element(ElfPet& p, int element) {
    p.elementalAttributes[0] = element;
    p.elementalAttributes[1] = 0;
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 100, int atk = 200) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, 99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1530 743 反转成功 → 下 1 回合先制+1（劈天狂击 [1,1]）══
    {
        // a) 正例：对手 +2 → 反转翻 -2 → R2 慢速自身先手，R3 过期回对手
        {
            ScenarioBattle b(make_party(self_pet({kPiTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS,
                  "T1530.a 前置：对手 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 劈天狂击
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2,
                     "T1530.a 743：对手 +2 被反转为 -2");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1530.a 743：R2 先制+1 → 慢速自身先手");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1530.a 743：R3 窗过期 → 对手（speed 300）先手");
        }
        // b) 负例：对手无提升（反转 NOTHING）→ 不武装窗 → R2 对手先手
        {
            ScenarioBattle b(make_party(self_pet({kPiTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1530.b 743 门：无提升不反转 → R2 对手先手");
        }
        std::cout << "  [T1530] 743 反转先制窗 通过\n";
    }

    // ══ T1531 1491 遇天敌 → 选择期先制+1（临风利爪 [1491,956]|[250]）══
    {
        // a) 正例：我水 2 / 对手草 1（草克水=天敌）→ R1 慢速自身先手
        {
            ElfPet me = self_pet({kLinFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_element(me, kWater);
            set_element(foe, kGrass);
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 临风利爪
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1531.a 1491：天敌先制+1 → 慢速自身 R1 先手");
        }
        // b) 负例：同属性（无克制）→ 不加先制 → 对手先手
        {
            ElfPet me = self_pet({kLinFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_element(me, kWater);
            set_element(foe, kWater);
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1531.b 1491 门：非天敌 → 对手（speed 300）先手");
        }
        std::cout << "  [T1531] 1491 天敌先制 通过\n";
    }

    std::cout << "scenario_1530 PASS (J66 反转先制族 743/1491)" << std::endl;
    return 0;
}
