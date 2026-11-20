// 场景 026：必先（guaranteed-first）机制 —— 先手权双参照（必先等级 → 先制 → 速度）。
//   必先由 grant_guaranteed_first 注册一个"下一回合先手权时点"的 once 回合类效果置 ws.guaranteed_first。
//   验证：
//     B  必先压先制+速度：0号(慢+先制0) 得必先 → 下一回合先手权压过 1号(先制+3+高速)
//     C  比必先等级：双方都有必先时, 等级高的先（1号 tier2 > 0号 tier1 → 1号先）
//     D  断回合移除必先：必先注册后被 break_round_effects 断掉 → 未生效, 回到先制（1号先）
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {
constexpr int kSkillZhuangJi = 10001;  // 撞击：先制0
constexpr int kSkillGuangShanJi = 10630;  // 光闪击：先制+3
}

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 0号慢+先制0; 1号快+光闪击(先制+3)。无必先时 1号先手(SEER_ROBOT_2)。
    auto pet0 = scenario::make_pet(0, "慢方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                               kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    auto pet1 = scenario::make_pet(1, "快方", {kSkillGuangShanJi, kSkillZhuangJi, kSkillZhuangJi,
                                               kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/900, /*atk=*/100, /*hp=*/3000);
    auto run_round = +[](scenario::ScenarioBattle* b) {
        b->submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b->submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b->drive();
    };

    // ---- B) 必先压先制+速度 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        run_round(&battle);  // 第1回合(无必先): 1号先制+3 → 1号先
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);
        grant_guaranteed_first(&battle.ctx(), 0, 1);  // 0号得"下一回合必先"(tier1)
        run_round(&battle);  // 第2回合: 必先起火 → 0号先(压过1号先制+3)
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1);
        std::cout << "  [B 必先压先制] 0号慢+先制0 得必先tier1 -> 先手权 SEER_ROBOT_1(0号)"
                  << " (压过1号速900+先制+3)\n";
    }

    // ---- C) 比必先等级 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        grant_guaranteed_first(&battle.ctx(), 0, 1);  // 0号 tier1
        grant_guaranteed_first(&battle.ctx(), 1, 2);  // 1号 tier2
        run_round(&battle);
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);  // tier2 > tier1 → 1号
        std::cout << "  [C 比必先等级] 0号tier1  vs  1号tier2 -> 1号先 (SEER_ROBOT_2)\n";
    }

    // ---- D) 断回合移除必先 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        run_round(&battle);                          // 第1回合: 1号先
        grant_guaranteed_first(&battle.ctx(), 0, 1); // 0号得下一回合必先
        break_round_effects(&battle.ctx(), 0);       // 对手断0号回合 → 必先效果被移除
        run_round(&battle);                          // 第2回合: 必先没了 → 1号先
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);
        std::cout << "  [D 断回合移除必先] 必先被 break -> 未生效, 1号先(SEER_ROBOT_2)\n";
    }

    std::cout << "scenario_026 PASS (必先：压先制/速度 + 比等级 + 可被断回合移除)\n";
    return 0;
}