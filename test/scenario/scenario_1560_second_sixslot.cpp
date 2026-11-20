// 场景 1560 —— J69 后手六槽族（1 宏，moves_lib/j_second_sixslot.cpp）
//（2026-10-03，worktree ../agents/j69；号段 1560-1569 → T1560 两块）
//
// 断言口径：
//   - 载体迷惑之光 22238 [483]|[-1,0,-1,0,-1,-1]：属性技（category 4，零伤害零随机
//     ——向量断言完全确定，无带宽）；
//   - 语义序→引擎槽映射：[-1,0,-1,0,-1,-1]（攻,防,特攻,特防,速,命中）→ 引擎槽
//     0/1/4/5 各 -1、槽 2/3（防/特防）不动；
//   - 后手翻倍 = 慢速自身（speed 100 vs 300）出招 → 各槽 -2；
//   - 免弱路径同 J48 负值分支（stat_drop 原语内查 STAT_DROP），场景不另造负例。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMiHuo = 22238;   // 迷惑之光[483]|[-1,0,-1,0,-1,-1]：属性技（后手×2）
constexpr int kPaiDa = 10004;   // 拍打（占位）

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

// 六槽期望断言（引擎槽序 0攻 1特攻 2防 3特防 4速 5命中）。
void check_levels(BattleContext& ctx, int slot0, int slot1, int slot2, int slot3,
                  int slot4, int slot5, const std::string& tag) {
    const int expect[6] = {slot0, slot1, slot2, slot3, slot4, slot5};
    for (int i = 0; i < 6; ++i) {
        CHECK_EQ(ctx.ability_levels[1][i], expect[i],
                 tag + "：槽" + std::to_string(i) + "==" + std::to_string(expect[i]));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1560 483 对手六槽弱化·后手翻倍（迷惑之光 [-1,0,-1,0,-1,-1]）══
    {
        // a) 先手（speed 300）：幅度 ×1 → 槽 0/1/4/5 = -1，槽 2/3 = 0
        {
            ScenarioBattle b(make_party(self_pet({kMiHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_levels(ctx, -1, -1, 0, 0, -1, -1, "T1560.a 483 先手");
        }
        // b) 后手（speed 100）：幅度 ×2 → 槽 0/1/4/5 = -2
        {
            ScenarioBattle b(make_party(self_pet({kMiHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_levels(ctx, -2, -2, 0, 0, -2, -2, "T1560.b 483 后手翻倍");
        }
        std::cout << "  [T1560] 483 后手六槽弱化 通过\n";
    }

    std::cout << "scenario_1560 PASS (J69 后手六槽族 483)" << std::endl;
    return 0;
}
