// 场景 1730 —— J83 先手改档族（1 宏，moves_lib/j_first_slot_change.cpp）
//（2026-10-03，worktree ../agents/j83；号段 1730-1739 → T1730 两块）
//
// 断言口径：
//   - 载体利爪贯骨击 15393 [122,148]|[1,100,-1,1,100,-2]：物150 prio0；
//     122=[1,100,-1]（先手→对方特攻槽 -1，100% 档确定）；148=[1,100,-2]（后手镜像
//     版，已注册 batch_b——负例断言直接用它：慢速自身 → 对方特攻槽 -2）；
//   - 双门互补断言：先手 a) 对方槽1==-1；后手 b) 对方槽1==-2（122 不触发、148 触发
//     ——门分离的可观测证明）；链上再无其他效果。
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
constexpr int kLiZhao = 15393;  // 利爪贯骨击[122,148]|[1,100,-1,1,100,-2]：物150（先手-1/后手-2 互补）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(1, "己方", skills, speed, 1, kMaxHp);
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

    // ══ T1730 122 先手改档 / 148 后手镜像（利爪贯骨击）══
    {
        // a) 先手（speed 300）：122 触发 → 对方槽1（特攻）-1
        {
            ScenarioBattle b(make_party(self_pet({kLiZhao, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][1], -1,
                     "T1730.a 122：先手 → 对方特攻槽 -1（实测 " +
                         std::to_string(ctx.ability_levels[1][1]) + "）");
        }
        // b) 后手（speed 100）：122 不触发；148 镜像触发 → 对方槽1 -2
        {
            ScenarioBattle b(make_party(self_pet({kLiZhao, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][1], -2,
                     "T1730.b 148 镜像：后手 → 对方特攻槽 -2（实测 " +
                         std::to_string(ctx.ability_levels[1][1]) + "）");
        }
        std::cout << "  [T1730] 122 先手改档 通过\n";
    }

    std::cout << "scenario_1730 PASS (J83 先手改档族 122)" << std::endl;
    return 0;
}
