// 场景 A：南霜·大雪纷飞 条件先制 → 决定先手权。
//
// 手动构造：南霜速度极低(100)、对手速度极高(300)。南霜用「大雪纷飞」(28319)，
// 其固有效果 2000「若对手处于能力提升状态则先制+1且必定命中」；
// 对手用「撞击」(10001，无效果，priority 0)。
// 双方手动挂相同能力提升 → 南霜 2000 触发先制+1 → 低速翻盘先手。
// 对照组（无能力提升）：高速对手先手。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillDaXueFenFei = 28319;  // 大雪纷飞（南霜）：固有效果 2000 条件先制+1、必中
constexpr int kSkillZhuangJi = 10001;     // 撞击：无效果，priority 0

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(0, "南霜",
                                   {kSkillDaXueFenFei, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/200, /*hp=*/500);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/300, /*atk=*/200, /*hp=*/500);

    // 对照组：无能力提升 → 2000 不触发 → 高速对手先手
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2);  // 高速对手先手
        std::cout << "  [control] no boost -> opponent first (speed 300)" << std::endl;
    }

    // 主场景：双方能力提升 → 南霜 2000 条件先制 +1 → 低速南霜翻盘先手
    // ⚠️ 能力等级本体已迁 `BattleContext::ability_levels`（on-stage 作用域，**开战会清零**）
    //    → 预置必须放在 `start()` 之后（2026-09-16 改造）。
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            battle.ctx().ability_levels[0][i] = 2;
            battle.ctx().ability_levels[1][i] = 2;
        }
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 单步模式：收下 op1 后逐步推进，在 MOVE_RIGHT 结算后停下，dump 时点
        battle.ctx().debug_step_mode = true;
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.step_past(State::BATTLE_FIRST_MOVE_RIGHT);

        // 关键时点断言：南霜(SEER_ROBOT_1) 先手 + 先制等级
        REQUIRE(battle.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1);
        REQUIRE(battle.ctx().ws.preemptive_level[0] >= 1);  // 南霜 +1
        std::cout << "  [main] boost -> 南霜 first (speed 100 + cond priority), "
                  << "preemptive_level0=" << battle.ctx().ws.preemptive_level[0]
                  << " preemptive_level1=" << battle.ctx().ws.preemptive_level[1] << std::endl;
        // 可开启查看全量时点状态：std::cout << battle.dump() << std::endl;
    }

    std::cout << "scenario_a PASS (南霜条件先制决定先手权)" << std::endl;
    return 0;
}
