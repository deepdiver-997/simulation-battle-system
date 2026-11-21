// 场景 J：万相乖离条件取消（2260 PP=0 触发）——连续使用无相谛逐条取消触发条件 + 威力提升。
//
// 机制：2260"使用的PP值为0的技能时...使用后触发万相乖离"。
// 万相乖离：永久取消第五技能（无相谛 37460）首条未取消效果的条件 + 提升50威力。
// 无相谛 parsed_units_ 顺序（按 rawEffectRecords）：[179, 700, 1083, 1257, 1248, 456]，
// 各自带条件（SameElement/FirstMove/SecondMove/TargetNoAnomaly/TargetHasAnomaly/TargetHpBelow）。
//
// 验证：每回合用一次无相谛（PP=0，2260 ignore_pp 可选 + force_execute 必中）→
//   第1次：取消 179 条件，power 0→50
//   第2次：取消 700 条件，power 50→100
//   第3次：取消 1083 条件，power 100→150
// 断言对应 parsed_units_[i].condition == None（"永久取消触发条件"落地到 EffectUnit 结构）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <effects/effect_unit.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillWuXiangDi = 37460;   // 无相谛（第五技能）
constexpr int kSkillZhuangJi = 10001;    // 撞击
constexpr int kFifthSlot = 4;            // 万相乖离作用于第五技能槽

// 无相谛放第5槽 + 真实 2260（stat_half/ignore_pp/force_execute/PP=0触发万相乖离）。
ElfPet make_wuxiang_pet(int speed, int hp) {
    return scenario::make_pet(0, "无为觉者",
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillWuXiangDi},
                              speed, /*atk=*/150, hp,
                              /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
    // 对手高血量：避免 3 回合内被无相谛（吸取1/3 + 威力提升后攻击）打死，导致战斗提前结束
    auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                               kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/90000);
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();

    const int expected_power[] = {50, 100, 150};  // 每次 +50
    for (int round = 1; round <= 3; ++round) {
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kFifthSlot);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        const auto& skill = battle.ctx().getPet(0).skills[kFifthSlot];
        assert(skill.power == expected_power[round - 1]);  // 威力累积 +50
        // 前 round 条效果条件已被永久取消（condition == None）
        for (int i = 0; i < round; ++i) {
            assert(skill.parsed_units_[i].condition == UnitCondition::None);
        }
        // 未取消的效果条件仍在（round 之后的第 0 条被取消的已变 None；抽查下一条仍带条件）
        if (round < 3) {
            assert(skill.parsed_units_[round].condition != UnitCondition::None);
        }
        std::cout << "  [round " << round << "] 无相谛 power=" << skill.power
                  << " 已取消条件数=" << round
                  << " (前" << round << "条 condition==None)" << std::endl;
    }

    std::cout << "scenario_j PASS (万相乖离：PP=0触发逐条取消无相谛条件 + 威力+50/次)"
              << std::endl;
    return 0;
}
