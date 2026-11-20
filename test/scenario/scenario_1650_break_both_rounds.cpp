// 场景 1650 —— J75 消双方回合族（2 宏，moves_lib/j_break_both_rounds.cpp）
//（2026-10-03，worktree ../agents/j75；号段 1650-1659 → T1650-T1651 两块）
//
// 断言口径：
//   - 1414 "消除成功"门 = 对手预置回合类效果（固化力场 28755 [1683]|[3]，R1 挂窗
//     R1..R3）→ R2 消除 SUCCESS → 双方各得异常 16；对照 = 全场无回合类效果 →
//     门关（NO_EFFECTS×2）→ 双方无异常；
//   - 1139 = 双侧附加中毒 [2,1,1]（指定 2 回合）：R1 末双方带毒；R2 末自然到期
//     （EVENT_ANOMALY_EXPIRED，FSM 内派发同 run 可见）→ 自身回满 5000 且毒解；
//     毒伤先扣、到期回满钳 5000，断言不受毒伤量影响；
//   - 两载体全纯链（35054 [1414]|[16] / 37739 [1139]|[2,1,1]）——无兄弟链激活风险。
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

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kXueRan = 35054;  // 血染战场 [1414]|[16]：特140（消双方→异常载体）
constexpr int kLongQi = 37739;  // 龙契·蛊 [1139]|[2,1,1]：特110（双侧毒+到期回满载体）
constexpr int kZhuoLie = 28331; // 灼烈之风 [1204]|[2]：对手 R1 施放 → NextRounds 必暴窗
                                //（ContinuousEffect 回合类效果，可被 1414 断掉）
constexpr int kMaxHp  = 5000;
constexpr int kAnom16 = 16;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1650 1414 消双方→双异常（血染战场 [16]）══
    {
        // a) 对手有回合类效果：R1 对手挂 1683 窗 → R2 消除 SUCCESS → 双方异常 16
        {
            ScenarioBattle b(
                make_party(mk_self({kXueRan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kZhuoLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：对手挂窗；己方拍打过渡
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // R2：血染战场（消双方回合类，对手侧 SUCCESS）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(0, kAnom16),
                  "T1650.a 1414：消除成功 → 自身异常 16");
            CHECK(ctx.has_active_abnormal_status(1, kAnom16),
                  "T1650.a 1414：消除成功 → 对手异常 16");
        }
        // b) 对照：全场无回合类效果 → 门关 → 双方无异常
        {
            ScenarioBattle b(
                make_party(mk_self({kXueRan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kAnom16),
                  "T1650.b 1414 对照：无回合类效果门关（自身无异常）");
            CHECK(!ctx.has_active_abnormal_status(1, kAnom16),
                  "T1650.b 1414 对照：门关（对手无异常）");
        }
        std::cout << "  [T1650] 1414 消双方→双异常 通过\n";
    }

    // ══ T1651 1139 双侧毒 2 回合 + 自身毒解回满（龙契·蛊 [2,1,1]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kLongQi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3000;                      // 预置缺口（回满可观测）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂双毒
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(0, 1) && ctx.has_active_abnormal_status(1, 1),
              "T1651 前置：双方均带中毒（2 回合窗）");
        // R2、R3 空转：毒窗（R1 起算 2 回合）在 R3 的回合扣减点自然到期
        // → EVENT_ANOMALY_EXPIRED → 自身毒解回满
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T1651.a 1139：自身中毒到期 → 回满（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(!ctx.has_active_abnormal_status(0, 1),
              "T1651.a 1139：自身毒已解除");
        std::cout << "  [T1651] 1139 双侧毒+到期回满 通过\n";
    }

    std::cout << "scenario_1650 PASS (J75 消双方回合族 1414/1139)" << std::endl;
    return 0;
}
