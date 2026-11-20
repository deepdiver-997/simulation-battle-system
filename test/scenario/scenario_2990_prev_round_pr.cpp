// 场景 2990 —— T2 内核字段 prev_round_preemptive_right 快照语义
//（2026-10-05，决策清单 K4.3 用户裁定：内核加"上回合先手权"字段，
//  取代各效果自记 plugin_storage 的做法；任务单 T2）
//
// 断言口径：
//   - 开局快照 = NONE；
//   - RC1 后快照 = R1 先手权（速度 300>100 → ROBOT_1）；
//   - R2 内 FMR 已把 preemptive_right 清零并重判（嗑药方后手 → ROBOT_2），
//     此时快照仍 = R1 值 —— **T2 核心语义**（快照跨 FMR memset 存活）；
//   - RC2 后快照更新为 R2 值。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：双方攻击载体

ElfPet speed_pet(int id, const std::string& name, int player, int speed) {
    ElfPet p = make_pet(player, name, {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/10, /*hp=*/500);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    // player0 速度 300、player1 速度 100 → 同为攻击时 player0 先手；
    // player0 备 3 瓶初级体力药剂（300011）供 R2 嗑药翻后手。
    ScenarioBattle battle(make_party(speed_pet(1, "快方", 0, 300)),
                          make_party(speed_pet(2, "慢方", 1, 100)),
                          MedicineStock{{300011, 3}}, {});

    CHECK(battle.ctx().prev_round_preemptive_right == PreemptiveRight::NONE,
          "开局：快照应为 NONE");

    // ⚠️ 驱动方式：双方操作收齐后 run() 会一口气跑完整回合（CHOOSE 枚举序在 FMR 之前，
    // submit 返回时已停在下一回合 CHOOSE）——要读"回合中"状态必须先开 debug_step_mode
    // 再提交第二手，之后逐步 step。
    // ── R1：双方攻击 ──
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.ctx().debug_step_mode = true;
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
    const PreemptiveRight pr_r1 = battle.ctx().preemptive_right;
    CHECK(pr_r1 == PreemptiveRight::SEER_ROBOT_1, "R1：速度 300>100，先手权应归 player0");
    battle.drive();   // 跑完 R1 剩余时点 → RC1 → 停在 R2 的 CHOOSE
    CHECK(battle.ctx().prev_round_preemptive_right == pr_r1,
          "RC1 后：快照应 = R1 先手权");

    // ── R2：player0 嗑药（后手）→ 先手权翻为 ROBOT_2 ──
    battle.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
    battle.ctx().debug_step_mode = true;
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
    CHECK(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
          "R2：player0 嗑药后手 → 当回合先手权应为 ROBOT_2");
    CHECK(battle.ctx().prev_round_preemptive_right == pr_r1,
          "T2 核心：R2 内（FMR 已清零重判 pr）快照仍 = R1 先手权");
    battle.drive();
    CHECK(battle.ctx().prev_round_preemptive_right == PreemptiveRight::SEER_ROBOT_2,
          "RC2 后：快照应更新为 R2 先手权");

    return 0;
}
