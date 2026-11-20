// 场景 5500 —— J221 下降双倍反馈+免弱窗族（1260，moves_lib/j_drop_reflect_immune.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5500-5509）
//
// 断言口径（精确槽位 + 原语返回档 + 窗界）：
//   - 1260（异电鬼面 27510 [1260,48,776]|[3,4,2]）：{0}=3 回合窗；链兄弟 48 自免异常、
//     776 攻击翻倍，均与本族断言面无关（属性技无伤害阶段）。
//   - a) 自身防御 -2 → 反馈双倍：对手防御槽 -4；自身下降**不清除**（仍 -2）；
//        反馈成功 → STAT_DROP 免疫票生效（后续 stat_drop 返回 IMMUNE、槽不动）。
//   - b) 门闭：自身无负槽 → 无反馈、不授票（后续 stat_drop 返回 SUCCESS → -1）。
//   - c) 窗界：rounds=3 → R2 内仍 IMMUNE；R5（register+3 已过）→ SUCCESS。
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
constexpr int kYiDian  = 27510;  // [1260,48,776]|[3,4,2]（1260 rounds=3）
constexpr int kDefSlot = 2;      // 能力等级槽序：2=防御
constexpr int kMaxHp   = 99999;
constexpr int kRounds  = 3;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kYiDian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 一回合双方都不出手（只推进回合，让窗口票自然过期）。
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

    // ══ T5500 1260 下降双倍反馈 + 反馈成功授免弱窗 ══
    {
        {   // (a) 自身防御 -2 → 对手 -4；自身不清；免弱票生效
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 2, /*actor=*/1)),
                     static_cast<int>(StatDropResult::SUCCESS),
                     "T5500.a 前置：自身防御 -2 铺设");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -2, "T5500.a 前置：自身防御 = -2");

            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();

            CHECK_EQ(ctx.ability_levels[1][kDefSlot], -4,
                     "T5500.a 1260：对手防御双倍反馈 -4（实测 " +
                         std::to_string(ctx.ability_levels[1][kDefSlot]) + "）");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -2,
                     "T5500.a 1260：自身下降不清除（实测 " +
                         std::to_string(ctx.ability_levels[0][kDefSlot]) + "）");
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 1, /*actor=*/1)),
                     static_cast<int>(StatDropResult::IMMUNE),
                     "T5500.a 1260：反馈成功 → STAT_DROP 免疫票生效");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -2,
                     "T5500.a 1260：免疫票挡下后自身防御仍 -2");
        }
        {   // (b) 自身无下降 → 反馈不落地 → 不授票
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();

            CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0,
                     "T5500.b 1260 门闭：无自身下降 → 对手防御不变（实测 " +
                         std::to_string(ctx.ability_levels[1][kDefSlot]) + "）");
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 1, /*actor=*/1)),
                     static_cast<int>(StatDropResult::SUCCESS),
                     "T5500.b 1260 门闭：无反馈成功 → 未授免弱票（弱化照常落地）");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -1,
                     "T5500.b 1260 门闭：自身防御被压到 -1");
        }
        {   // (c) 窗界：rounds=3 → R2 仍免疫；R5 已过期
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 2, /*actor=*/1)),
                     static_cast<int>(StatDropResult::SUCCESS),
                     "T5500.c 前置：自身防御 -2 铺设");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();

            idle_round(b);   // → R2
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 1, /*actor=*/1)),
                     static_cast<int>(StatDropResult::IMMUNE),
                     "T5500.c 1260 窗内（R2）：免弱票仍生效");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -2,
                     "T5500.c 1260 窗内（R2）：自身防御仍 -2");

            idle_round(b);   // → R3
            idle_round(b);   // → R4（register_round + 3 起过期）
            idle_round(b);   // → R5
            CHECK_EQ(static_cast<int>(stat_drop(&ctx, 0, kDefSlot, 1, /*actor=*/1)),
                     static_cast<int>(StatDropResult::SUCCESS),
                     "T5500.c 1260 窗外（R5）：免弱票已过期，弱化照常落地");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], -3,
                     "T5500.c 1260 窗外（R5）：自身防御 -2→-3（实测 " +
                         std::to_string(ctx.ability_levels[0][kDefSlot]) + "）");
            (void)kRounds;
        }
        std::cout << "  [T5500] 1260 双倍反馈 / 反馈成功授免弱 / 窗界 通过\n";
    }

    std::cout << "scenario_5500 PASS (J221 下降双倍反馈+免弱窗族)" << std::endl;
    return 0;
}
