// 场景 1050 —— J20 承伤/输出修正族（3 宏，moves_lib/j_dmg_mod_family.cpp）
//（2026-10-02，worktree ../agents/j20；号段 1050-1059 → T1050-T1052 三块）
//
// 断言口径：
//   - 1265 用同刀对照（对拍打 atk=1000 两刀）：R1 减 50%、R2 全价 → 比值带
//     2×[0.85,1/0.85]=[170,235]%（85%~100% 浮动双刀展开）；
//   - 566 用同威力对照（混元黑光斩 vs 神风烈踢，双 cat1 pow150，先基线后 566）：
//     防御项 ×(100/80) → 比值带 1.25×[0.85,1/0.85]=[106,147]%；
//   - 567 用同技双局对照（碎灭猛击，天敌局 vs 同属性局）：比值带 2×[0.85,1/0.85]
//     =[170,235]%；
//   - 载体链排查：26650 虚无残魂链上 191/47=免票、693=己方增伤窗（不碰承伤面）；
//     15469/15590 单效果载体。
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

constexpr int kPaiDa   = 10004;   // 拍打：物40（对手探针攻击）
constexpr int kXuWu    = 26650;   // 虚无残魂[191,47,693,1265]|[4,4,2,100,1,50]：属（1次-50%）
constexpr int kHeiGuang = 15469;  // 混元黑光斩[566]|[20]：物150（忽略20%防御）
constexpr int kFengLie = 10181;   // 神风烈踢：物150 无效果（566 基线）
constexpr int kSuiMie  = 15590;   // 碎灭猛击[567]：物115（天敌翻倍，零参）
constexpr int kGrass = 1, kWater = 2;
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

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void set_element(ElfPet& p, int element) {
    p.elementalAttributes[0] = element;
    p.elementalAttributes[1] = 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1050 1265 下1次受到的攻击伤害减少50%（虚无残魂 [1265]|[1,50]）══
    {
        ScenarioBattle b(make_party(self_pet({kXuWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（属）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    后手拍打
        b.drive();
        const int taken_r1 = kMaxHp - ctx.getPet(0).hp;
        CHECK(taken_r1 > 100, "T1050.a 前置：R1 吃到正伤");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 全价拍打
        b.drive();
        const int taken_r2 = (kMaxHp - taken_r1) - ctx.getPet(0).hp;
        const int ratio_x100 = taken_r2 * 100 / taken_r1;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T1050.a 1265：R1 减半 R2 全价（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；R1=" + std::to_string(taken_r1) +
                  " R2=" + std::to_string(taken_r2) + "）");
        std::cout << "  [T1050] 1265 计数减伤窗 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T1051 566 攻击时忽略对手20%防御（混元黑光斩 vs 神风烈踢同威力对照）══
    {
        ScenarioBattle b(make_party(self_pet({kFengLie, kHeiGuang, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/300)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 基线（无 566）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int base = kMaxHp - ctx.getPet(1).hp;
        CHECK(base > 100, "T1051 前置：基线正伤");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 混元黑光斩
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int boosted = (kMaxHp - base) - ctx.getPet(1).hp;
        const int ratio_x100 = boosted * 100 / base;
        CHECK(ratio_x100 >= 106 && ratio_x100 <= 147,
              "T1051 566：忽略 20% 防御 ≈ +25% 伤（比值 " + std::to_string(ratio_x100) +
                  "%，带 106~147；基线=" + std::to_string(base) +
                  " 566=" + std::to_string(boosted) + "）");
        std::cout << "  [T1051] 566 忽略防御 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T1052 567 天敌伤害翻倍（碎灭猛击，天敌局 vs 同属性局对照）══
    {
        int d_norm = 0, d_pred = 0;
        {
            ScenarioBattle b(make_party(self_pet({kSuiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_norm = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(d_norm > 100, "T1052 前置：同属性局正伤");
        {
            ElfPet me = self_pet({kSuiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/300, /*atk=*/300);
            set_element(me, kWater);
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_element(foe, kGrass);   // 草克水 → 对手是天敌
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_pred = kMaxHp - ctx.getPet(1).hp;
        }
        const int ratio_x100 = d_pred * 100 / d_norm;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T1052 567：天敌局伤害翻倍（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；同属=" + std::to_string(d_norm) +
                  " 天敌=" + std::to_string(d_pred) + "）");
        std::cout << "  [T1052] 567 天敌翻倍 通过 (" << ratio_x100 << "%)\n";
    }

    std::cout << "scenario_1050 PASS (J20 承伤/输出修正族 1265/566/567)" << std::endl;
    return 0;
}
