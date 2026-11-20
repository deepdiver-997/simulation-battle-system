// 场景 1670 —— J77 低血门/双倍反转族（1015/896，moves_lib/j_lowhp_double_reversal.cpp）
//（2026-10-03，worktree ../agents/j77；号段 1670-1679 → T1670-T1671 两块）
//
// 断言口径：
//   - 1015 低血门（27919 [3] 属性技）：自身 hp 低于对手 → 回 max/3（缺口开足防钳）；
//     对照自身 hp ≥ 对手 → 不回。
//   - 896 双倍反转（30788 零参）：自身槽 [0]=-1、[2]=-3、[4]=+2 → 反转后
//     [0]=+2、[2]=+6、[4] 不动、其余 0（全确定）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa  = 10004;   // 拍打（填充刀）
constexpr int kShouWen = 27919;  // 兽纹威颜 [1015]|[3] 属性技（低血回 max/3）
constexpr int kHunTong = 30788;  // 混同神力 [896]|[] 特130（双倍反转）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 20000) {
    ElfPet p = make_pet(1, "低血族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1670 1015 低血门（27919 [3]）══
    {
        {   // (a) 自身 hp 低于对手 → 回 max/3 = 6666（R1 挨刀开缺口）
            ScenarioBattle b(
                make_party(self_pet({kShouWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1, /*hp=*/20000)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/30000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挨刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            CHECK(hp_low < 14000 && hp_low > 2000,
                  "T1670.a 前置：缺口已开（hp=" + std::to_string(hp_low) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 兽纹威颜
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < ctx.getPet(1).hp, "T1670.a 前置：自身仍低于对手");
            CHECK_EQ(ctx.getPet(0).hp - hp_low, 20000 / 3,
                     "T1670.a 1015：低血 → 回 max/3（实测 +" +
                         std::to_string(ctx.getPet(0).hp - hp_low) + "）");
        }
        {   // (b) 对照：自身 hp 高于对手 → 不回（R1 拍打先削对手血）
            ScenarioBattle b(
                make_party(self_pet({kShouWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/3000, /*hp=*/99998)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/1, /*hp=*/99998)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 拍打削对手
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp > ctx.getPet(1).hp,
                  "T1670.b 前置：自身 hp 更高（" + std::to_string(ctx.getPet(0).hp) + " vs " +
                      std::to_string(ctx.getPet(1).hp) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 兽纹威颜
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 99998,
                     "T1670.b 1015 门：自身不低血不回血（hp=" +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1670] 1015 低血门 通过\n";
    }

    // ══ T1671 896 双倍反转（30788 零参）══
    {
        ScenarioBattle b(
            make_party(self_pet({kHunTong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 0, 2, -3) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 0, 4, +2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T1671 896：槽0 -1 → +2");
        CHECK_EQ(ctx.ability_levels[0][2], 6, "T1671 896：槽2 -3 → +6");
        CHECK_EQ(ctx.ability_levels[0][4], 2, "T1671 896：正槽不动");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T1671 896：零槽不动");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T1671 896：对手不受影响");
        std::cout << "  [T1671] 896 双倍反转 通过\n";
    }

    std::cout << "scenario_1670 PASS (J77 低血门/双倍反转族)" << std::endl;
    return 0;
}
