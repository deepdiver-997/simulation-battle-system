// 场景 2540 —— J163 阈值响应族三（949/1041，moves_lib/j_low_damage_gates2.cpp）
//（2026-10-04，worktree ../agents/j163；号段 2540-2549）
//
// 断言口径（伤害实测基准带法：预刀测 x1，断言用 ±18% 浮动带）：
//   - 949 回满（舍身重斩 31366 [300] 物130，atk=1 → 红 ~95<300）：
//       R1 混沌利爪@1800 造缺口（-1370..-2261）；R2 舍身重斩 → 恰回满 5000。
//       对照：atk=1000（红>300）→ 不回。
//   - 1041(A) 自身免伤票（残影绝尘断 30234 [1052,1041]|[280,1] 特160，atk=1 → 红<280）：
//       R2 施放出票 → R3 对手拍打被免疫（hp 不动）；R4 再拍打正常掉血（次数耗尽）。
//       对照：atk=1000（红≥280）→ 无票，R3 拍打正常掉血。
//       （1052 消强门：对手无提升 → 良性 no-op。）
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打（foe 标准刀 atk=800）
constexpr int kHunDunDao = 17201; // 混沌利爪（foe 大威力削血 idx0）
constexpr int kSheShen = 31366;   // 舍身重斩 [949]|[300]（物130）
constexpr int kCanYing = 30234;   // 残影绝尘断 [1052,1041]|[280,1]（特160）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk, int hp = 5000) {
    ElfPet p = make_pet(1, "阈值族", skills, /*speed=*/300, atk, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/800, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2540 949 低伤回满 ══
    {
        {   // (a) atk=1：红<300 → 回满
            ScenarioBattle b(make_party(self_pet({kSheShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 1)),
                             make_party(foe_pet({kHunDunDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R1 造缺口
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            CHECK(before < 5000, "T2540.a 前置：缺血（before=" +
                                      std::to_string(before) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 舍身重斩
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int red = ctx.resolvedDamage.final;
            CHECK(red < 300, "T2540.a 前置：红<300（red=" + std::to_string(red) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T2540.a 949：恰回满 5000");
        }
        {   // (b) atk=1000：红≥300 → 不回
            ScenarioBattle b(make_party(self_pet({kSheShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int red = ctx.resolvedDamage.final;
            CHECK(red >= 300, "T2540.b 前置：红≥300（red=" + std::to_string(red) + "）");
            CHECK_EQ(ctx.getPet(0).hp, before, "T2540.b 949 对照：不回");
        }
        std::cout << "  [T2540] 949 低伤回满+对照 通过\n";
    }

    // ══ T2541 1041(A) 低伤自免票 ══
    {
        {   // (a) atk=1：红<280 → 出票 → R3 拍打被免疫、R4 正常掉血
            ScenarioBattle b(make_party(self_pet({kCanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 出票
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int hp_with_ticket = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R3 拍打被免疫
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp_with_ticket,
                     "T2541.a 1041：免疫票挡下拍打（hp 不动）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R4 票已耗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp_with_ticket,
                  "T2541.a 票耗尽：正常掉血");
        }
        {   // (b) atk=1000：红≥280 → 无票，R3 拍打正常掉血
            ScenarioBattle b(make_party(self_pet({kCanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < before, "T2541.b 对照：无票正常掉血");
        }
        std::cout << "  [T2541] 1041(A) 自身免伤票+对照 通过\n";
    }

    std::cout << "scenario_2540 ALL PASS" << std::endl;
    return 0;
}
