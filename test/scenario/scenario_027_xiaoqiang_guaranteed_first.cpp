// 场景 027：消强成功 → 下一回合必先（技能效果的判定逻辑，对应 moves_lib effect_xiaoqiang_bixian）。
//   "第五技能的消强"：清除对手能力提升，成功(清掉>0)则授予下一回合必先。
//   此处直接用内核原语复现该效果的条件分支（moves_lib 里的 effect_xiaoqiang_bixian 就是这套逻辑）：
//     A 消强成功: 对手有提升(攻击+2) → clear_stat_boosts>0 → 授必先 → 下一回合 0号先（压1号先制+3）
//     B 消强未成功: 对手无提升 → clear_stat_boosts=0 → 不授必先 → 下一回合 1号先
//   0号慢+先制0; 1号快+光闪击(先制+3)。
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {
constexpr int kSkillZhuangJi = 10001;
constexpr int kSkillGuangShanJi = 10630;  // 先制+3

// 复现 moves_lib effect_xiaoqiang_bixian 的条件逻辑。
void do_xiaoqiang_bixian(BattleContext* ctx, int owner, int opp) {
    if (clear_stat_boosts(ctx, opp) > 0) {          // 消强成功
        grant_guaranteed_first(ctx, owner, 1);       // 下一回合必先
    }
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    auto pet0 = scenario::make_pet(0, "消强方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                 kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    auto pet1 = scenario::make_pet(1, "被消强方", {kSkillGuangShanJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/900, /*atk=*/100, /*hp=*/3000);
    auto run_round = +[](scenario::ScenarioBattle* b) {
        b->submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b->submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b->drive();
    };

    // ---- A) 消强成功(对手有提升) → 下一回合必先压过先制+3 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        run_round(&battle);  // 第1回合: 1号先制+3 先
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);
        battle.ctx().ability_levels[1][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] = 2;  // 对手有提升
        do_xiaoqiang_bixian(&battle.ctx(), /*owner=*/0, /*opp=*/1);  // 消强成功→必先
        run_round(&battle);  // 第2回合: 0号必先 先(压过1号先制+3)
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1);
        REQUIRE(battle.ctx().ability_levels[1][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] == 0);  // 提升被清
        std::cout << "  [A 消强成功] 对手提升被清 + 授必先 -> 第2回合 0号先(SEER_ROBOT_1)\n";
    }

    // ---- B) 消强未成功(对手无提升) → 不授必先 → 1号先 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        run_round(&battle);
        do_xiaoqiang_bixian(&battle.ctx(), /*owner=*/0, /*opp=*/1);  // 对手无提升 → 消强未成功 → 不授必先
        run_round(&battle);  // 第2回合: 无必先 → 1号先制+3 先
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);
        std::cout << "  [B 消强未成功] 对手无提升 -> 不授必先 -> 第2回合 1号先(SEER_ROBOT_2)\n";
    }

    std::cout << "scenario_027 PASS (消强成功→下一回合必先；消强未成功→不触发必先)\n";
    return 0;
}