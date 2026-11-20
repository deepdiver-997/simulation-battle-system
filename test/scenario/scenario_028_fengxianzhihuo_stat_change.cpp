// 场景 028：不灭·艾恩斯 奉献之火(31269) —— effect 5 "{N}%改变对手属性等级"。
//   奉献之火 side_effect=[5,5] args=[0,100,-2, 1,100,-2]：攻击对手 + 100%令对手攻击-2、特攻-2。
//   通用 effect_5 走 CoreApi stat_change 真原语。验证：艾恩斯用奉献之火 → 对手攻击/特攻等级 -2。
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {
constexpr int kSkillFengXianZhiHuo = 31269;  // 奉献之火：120特攻 + effect_5×2(攻/-2, 特攻/-2, 100%)
constexpr int kSkillZhuangJi = 10001;
}

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(3467, "不灭·艾恩斯",
                                   {kSkillFengXianZhiHuo, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/300, /*atk=*/150, /*hp=*/3000);
    pet0.skills[0].must_hit = true;  // 保证命中才触发"技能使用成功时"
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/4000);
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    BattleContext& ctx = battle.ctx();

    // 奉献之火两个 effect_5(100%)生效：对手攻击+特攻各 -2
    CHECK_EQ(ctx.ability_levels[1][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)], -2,
             "奉献之火 effect_5: 对手攻击应 -2");
    CHECK_EQ(ctx.ability_levels[1][static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK)], -2,
             "奉献之火 effect_5: 对手特攻应 -2");
    std::cout << "  [奉献之火 effect_5] 对手攻击/特攻等级 -> "
              << ctx.ability_levels[1][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)]
              << " / " << ctx.ability_levels[1][static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK)]
              << " (-2/-2)\n";

    // 回归：负能力等级的伤害倍率 = 2/(2-|level|)。旧写法 2/(2+level) 在 -2 处**除零**
    // → int 溢出成 INT_MAX（对手 3000 体力会被一击打成 0）。此处对手(1)攻击等级 -2 后
    // 仍出手打过我方(0)：伤害应为"减半"量级（约 15），故我方体力只掉一点点。
    const int my_hp = ctx.getPet(0).hp;
    CHECK(my_hp > 2900 && my_hp <= 3000,
          "对手攻击等级 -2 时打出的是减半伤害（非 INT_MAX 溢出）");
    std::cout << "  [负等级倍率回归] 我方体力 -> " << my_hp << "/3000（-2 等级打出减半伤害）\n";

    std::cout << "scenario_028 PASS (艾恩斯奉献之火 effect_5: 100%改变对手攻击/特攻 -2, 走 stat_change 真原语)\n";
    return 0;
}