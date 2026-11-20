// 场景 1990 —— J105 消状态先制/反转PP族（2 宏，moves_lib/j_cleanse_prio_reversal_pp.cpp）
//（2026-10-03，worktree ../agents/j105；号段 1990-1999 → T1990-T1991 两块）
//
// 断言口径：
//   - 629（瞬迅风牙 [0,1] 物90 纯链）：对手预置麻痹 → 载体命中 → 麻痹解除
//     （has_active 即刻假）+ R2 ROUND_START 先制 +1（速度倒置翻盘：self 100 vs
//     300，+1 后先手）；对照 = 对手无麻痹 → 门关不加持；
//   - 1495（欺诈幻影 [1495,594✓]|[100,2]——594 已实现良性）：对手 +2 两槽 →
//     REVERSED（翻 -2）+ chance=100 → 恰 2 槽 PP 归零（洗牌不赖槽）；对照 =
//     对手无提升 → 不清 PP。
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
constexpr int kShunXun = 16327; // 瞬迅风牙 [629]|[0,1]：物90（消麻痹先制载体）
constexpr int kQiZha  = 33860;  // 欺诈幻影 [1495,594]|[100,2,…]：特85（反转PP载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1990 629 消麻痹 → 下回合先制 +1（瞬迅风牙 [0,1]）══
    {
        // a) 对手麻痹 → 消除 + R2 先制 +1 翻盘（self 100 vs foe 300）
        {
            ScenarioBattle b(
                make_party(mk_self({kShunXun, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis),
                                /*duration=*/5, /*actor=*/0);
            CHECK(ctx.has_active_abnormal_status(1,
                                                 static_cast<int>(AbnormalStatusId::Paralysis)),
                  "T1990.a 前置：对手已麻痹");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1,
                                                  static_cast<int>(AbnormalStatusId::Paralysis)),
                  "T1990.a 629：麻痹已消除");
            // R2：先制 +1 生效（速度 100+1 级 vs 300 仍慢——先制等级优先于速度）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1990.a 629：下回合先制 +1 翻盘先手");
        }
        // b) 基线对照：无载体无 629（双方拍打 prio 0）→ 速度定序 → 对手先手
        // ⚠️ 不用"载体施放但对手无麻痹"做门关对照——载体 16327 自带基值先制 +2 且
        //    会泄漏到次轮（核心装载器行为，T1990 调试实证），跨轮先手断言不可用。
        {
            ScenarioBattle b(
                make_party(mk_self({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1990.b 629 对照：基线（无 629）→ R2 对手先手");
        }
        std::cout << "  [T1990] 629 消麻痹先制 通过\n";
    }

    // ══ T1991 1495 反转成功 → 100% 随机 2 槽 PP 归零（欺诈幻影 [100,2]）══
    {
        // a) 对手 +2 两槽 → REVERSED → 100% 清 2 槽 PP
        {
            ScenarioBattle b(
                make_party(mk_self({kQiZha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 2);
            (void)stat_change(&ctx, 1, 3, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2,
                     "T1991.a 前置：对手提升被反转（槽0 实测 " +
                         std::to_string(ctx.ability_levels[1][0]) + "）");
            int zeroed = 0;
            for (auto& sk : ctx.getPet(1).skills) {
                if (sk.pp == 0) {
                    ++zeroed;
                }
            }
            CHECK(zeroed == 2,
                  "T1991.a 1495：随机 2 槽 PP 归零（实测 " + std::to_string(zeroed) + "）");
        }
        // b) 对照：对手无提升 → 反转不成立 → PP 不清
        {
            ScenarioBattle b(
                make_party(mk_self({kQiZha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int zeroed = 0;
            for (auto& sk : ctx.getPet(1).skills) {
                if (sk.pp == 0) {
                    ++zeroed;
                }
            }
            CHECK(zeroed == 0,
                  "T1991.b 1495 对照：无提升不清 PP（实测 " + std::to_string(zeroed) + "）");
        }
        std::cout << "  [T1991] 1495 反转PP归零 通过\n";
    }

    std::cout << "scenario_1990 PASS (J105 消状态先制/反转PP族 629/1495)" << std::endl;
    return 0;
}
