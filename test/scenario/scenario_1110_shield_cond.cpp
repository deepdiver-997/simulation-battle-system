// 场景 1110 —— J26 护盾/护罩条件族（3 宏，moves_lib/j_shield_cond.cpp）
//（2026-10-02，worktree ../agents/j26；号段 1110-1119 → T1110-T1112 三块）
//
// 断言口径：
//   - 1342 断 hood_bank_.total()（护罩账，与护盾账独立——847 共链加盾不污染）；
//   - 1120/1008 用 atk 档位控制阈值判定（atk=1 → 伤害 1~2 恒低于 280/200 阈；
//     atk=3000 → 伤害数千恒高于阈），断 shield_bank_.total()（生物护盾链上的 847
//     只加 1342 载体局用——1120/1008 载体链已排查无加盾共生效：1119=先制、1010=增伤窗）；
//   - DETECT 回调在伤害落地前（管线内副作用）——total() 在 drive() 后读，时点无碍。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物40（对手探针）
constexpr int kHuZhao = 27331;   // 生物护盾[885,1239,847,1342]|[1,2,2,400,400]：属（护罩400）
constexpr int kDiYu   = 30454;   // 八荒岩爆[1119,1120]|[1,2,280,200]：物85（伤害<280→盾200）
constexpr int kYuanLi = 31498;   // 源力光波[1010,1008]|[1,100,200,200]：特85（伤害>200→盾200）
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1110 1342 附加 400 点护罩（生物护盾 [1342]|[400]）══
    {
        ScenarioBattle b(make_party(self_pet({kHuZhao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 400,
                 "T1110 1342：护罩账 +400（实测 " +
                     std::to_string(ctx.getPet(0).hood_bank_.total()) + "）");
        std::cout << "  [T1110] 1342 附加护罩 通过\n";
    }

    // ══ T1111 1120 伤害<280 → 盾200（八荒岩爆 [1120]|[280,200]）══
    {
        // 正例：atk=1 → 伤害 1~2 < 280 → 盾 200
        {
            ScenarioBattle b(
                make_party(self_pet({kDiYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 200,
                     "T1111.a 1120：低伤 → 盾 200（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        // 负例：atk=3000 → 伤害 ≥280 → 无盾
        {
            ScenarioBattle b(
                make_party(self_pet({kDiYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/3000)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T1111.b 1120 门：高伤无盾（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        std::cout << "  [T1111] 1120 低伤护盾 通过\n";
    }

    // ══ T1112 1008 伤害>200 → 盾200（源力光波 [1008]|[200,200]）══
    {
        // 正例：atk=3000 → 伤害 > 200 → 盾 200
        {
            ScenarioBattle b(
                make_party(self_pet({kYuanLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/3000)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 200,
                     "T1112.a 1008：高伤 → 盾 200（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        // 负例：atk=1 → 伤害 1~2 ≤ 200 → 无盾
        {
            ScenarioBattle b(
                make_party(self_pet({kYuanLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T1112.b 1008 门：低伤无盾（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        std::cout << "  [T1112] 1008 高伤护盾 通过\n";
    }

    std::cout << "scenario_1110 PASS (J26 护盾/护罩条件族 1342/1120/1008)" << std::endl;
    return 0;
}
