// 场景 5520 —— J222 回合末反转提升窗族（1566，moves_lib/j_round_end_boost_reversal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5520-5529）
//
// 断言口径（精确槽位 + 逐回合快照）：
//   - 1566（邪翅送葬 28303 纯链 [1566]|[3]）：{0}=3 回合窗；载体 cat4 属性技无伤害面。
//   - a) R1 末：对手攻击 +2 → 反转 -2；其余槽不动。
//   - b) R2/R3 末：每回合重设 +2 → 各再反转 -2（"每回合结束后"逐回合生效）。
//   - c) R4：窗过期（register_round=1 + 3）→ 重设 +2 不再反转，保持 +2。
//   - d) 对照：对手无提升 → 反转空跑，等级不变。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kXieChi  = 28303;  // [1566]|[3]（纯链；3 回合窗）
constexpr int kAtkSlot = 0;      // 能力等级槽序：0=攻击
constexpr int kDefSlot = 2;      // 2=防御（旁证槽，应恒 0）
constexpr int kMaxHp   = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kXieChi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5520 1566 回合末反转对手提升（3 回合窗）══
    {
        {   // (a) R1 末反转 + (b) R2/R3 逐回合反转 + (c) R4 过期
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, kAtkSlot, 2) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                     "T5520.a R1 末：对手攻击 +2 反转 -2（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
            CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0,
                     "T5520.a R1 末：其余槽不动（防御仍 0）");

            // R2：重设 +2 → 末反转 -2
            REQUIRE(stat_change(&ctx, 1, kAtkSlot, 4) == StatChangeResult::SUCCESS);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2, "T5520.b R2 前置：对手攻击重设 +2");
            idle_round(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                     "T5520.b R2 末：再次反转 -2（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");

            // R3：同一窗口最后一回合
            REQUIRE(stat_change(&ctx, 1, kAtkSlot, 4) == StatChangeResult::SUCCESS);
            idle_round(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                     "T5520.b R3 末：窗口末回合仍反转 -2（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");

            // R4：register_round=1 + 3 → 已过期
            REQUIRE(stat_change(&ctx, 1, kAtkSlot, 4) == StatChangeResult::SUCCESS);
            idle_round(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2,
                     "T5520.c R4 末：窗过期 → 不再反转，保持 +2（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        }
        {   // (d) 对照：对手无提升 → 反转空跑
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                     "T5520.d 对照：对手无提升 → 反击空跑（攻击仍 0）");
            CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T5520.d 对照：防御仍 0");
        }
        std::cout << "  [T5520] 1566 回合末反转 / 逐回合 / 窗过期 / 无提升对照 通过\n";
    }

    std::cout << "scenario_5520 PASS (J222 回合末反转提升窗族)" << std::endl;
    return 0;
}
