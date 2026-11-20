// 场景 1470 —— J61 阈值先制窗族（3 宏，moves_lib/j_threshold_prio_window.cpp）
//（2026-10-03，worktree ../agents/j61；号段 1470-1479 → T1470-T1472 三块）
//
// 断言口径：
//   - 先制窗行为断言 = R2 的 preemptive_right：先制等级优先于速度（FSM 1747 判据）——
//     自身 speed=100 + 先制+1 vs 对手 speed=300 → 自身先手（SEER_ROBOT_1）；窗过期
//     R3 回 SEER_ROBOT_2；
//   - 阈值门方向 = 1029 伤害<阈值 / 1223 伤害>阈值（atk 档位制造两侧：atk=1 → 1~2、
//     atk=300/400 → 带 [280,329]/[244,287]，离阈值 200 均有裕量）；
//   - 878 先手施加断言 = R2 末对手 has_active_abnormal_status(害怕)；负例 = 自身慢速
//     （自身行动结束落在 SECOND_AFTER_ACTION → 不施加）；自身必须真出手（拍打）——
//     非出手方跳过动作桶（execute_registered_actions 守门）；
//   - 载体链污染排查：31298 高温晶核 [1029] / 33775 极光电球 [878] 单效果；34726
//     密集弹幕袭链尾 475=低伤必暴窗（已注册，见 T1471 注）——三载体 priority 全 0
//     （香雪纷飞 33913 priority=3 会污染先制断言，弃用）。
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

constexpr int kPaiDa   = 10004;   // 拍打：物40（探针攻击）
constexpr int kGaoWen  = 31298;   // 高温晶核[1029]|[200,1,1]：物130（伤害<200→下1回合先制+1）
constexpr int kXiangXue = 34726;  // 密集弹幕袭[1223,475]|[300,2,1,300,2]：特150 prio0
                                  // （伤害>300→下2回合先制+1；链尾475=低伤必暴窗，已注册，
                                  //  正例伤害高于其阈值不触发=良性、负例触发只改暴击不碰先制）
constexpr int kJiGuang = 33775;   // 极光电球[878]|[1,6]：特140（下1回合先出手→对手害怕）

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 害怕=6

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 100, int atk = 200) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, 99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1470 1029 伤害<200 → 下 1 回合先制+1（高温晶核 [200,1,1]）══
    {
        // a) 正例：atk=1（1~2 伤害 < 200）→ R2 自身先手（慢速翻盘），R3 过期回对手先手
        {
            ScenarioBattle b(make_party(self_pet({kGaoWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1470.a 1029：R2 先制+1 → 慢速自身先手");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1470.a 1029：R3 窗过期 → 对手（speed 300）回先手");
        }
        // b) 负例：atk=300（伤害带 [280,329] ≥ 200）→ 不触发 → R2 对手先手
        {
            ScenarioBattle b(make_party(self_pet({kGaoWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*atk=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1470.b 1029 门：伤害不低 → R2 对手（speed 300）先手");
        }
        std::cout << "  [T1470] 1029 低伤先制窗 通过\n";
    }

    // ══ T1471 1223 伤害>300 → 下 2 回合先制+1（密集弹幕袭 [300,2,1]）══
    {
        // a) 正例：atk=400（特伤带 [430,506] > 300）→ R2/R3 自身先手（窗 2 回合）
        {
            ScenarioBattle b(make_party(self_pet({kXiangXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*atk=*/400)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1471.a 1223：R2 先制+1 → 慢速自身先手");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 仍在窗内
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T1471.a 1223：R3 窗第 2 回合 → 仍自身先手");
        }
        // b) 负例：atk=1（1~2 ≤ 300）→ 不触发（链尾 475 必暴窗不碰先制断言）
        {
            ScenarioBattle b(make_party(self_pet({kXiangXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T1471.b 1223 门：伤害不高 → R2 对手先手");
        }
        std::cout << "  [T1471] 1223 高伤先制窗 通过\n";
    }

    // ══ T1472 878 下 1 回合先出手 → 对手害怕（极光电球 [1,6]）══
    {
        // a) 正例：自身 speed 300 先手 → R2 自身行动结束施加害怕
        {
            ScenarioBattle b(make_party(self_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T1472.a 878：R1 窗未开（NextRounds）不施加");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（须真出手）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T1472.a 878：R2 先出手 → 对手害怕");
        }
        // b) 负例：自身 speed 100 后手 → 不施加
        {
            ScenarioBattle b(make_party(self_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T1472.b 878 门：后手不施加（实测对手干净）");
        }
        std::cout << "  [T1472] 878 先手施加窗 通过\n";
    }

    std::cout << "scenario_1470 PASS (J61 阈值先制窗族 1029/1223/878)" << std::endl;
    return 0;
}
