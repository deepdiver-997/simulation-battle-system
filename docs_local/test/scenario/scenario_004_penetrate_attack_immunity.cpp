// 场景 D：法咒共鸣斩 699「无视攻击免疫」→ 穿透可穿盔。
//
// 我方用「法咒共鸣斩」(17589)：side_effect=[697 无视伤害限制, 699 无视攻击免疫]，
// 经 effect_meta 归为 Penetration 凭证（Skills.penetration_flags.ignore_attack_immunity）。
// 主场景：对手挂"可穿盔"（seal_attack，penetrable=true）→ 699 凭证穿透 → HIT。
// 对照组：对手挂"龙威"（penetrable=false，不可穿透）→ 被挡 → SKILL_INVALID。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillFaZhouGongMingZhan = 17589;  // 法咒共鸣斩：side_effect=[697 无视伤害限制, 699 无视攻击免疫]
constexpr int kSkillZhuangJi = 10001;            // 撞击：无效果，priority 0

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(0, "我方",
                                   {kSkillFaZhouGongMingZhan, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/200, /*atk=*/150, /*hp=*/800);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/2000);

    // 对照组：对手挂"龙威"（封攻+封属、不可穿透）→ 699 穿不动 → 我方技能被 SEALED
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        // 注入龙威：对手(施放方1)封我方(被封方0)攻击+属性技能，不可穿透
        seal_skill(&battle.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70003,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1, /*duration_rounds=*/0,
                   /*penetrable=*/false);
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);  // 龙威挡下
        std::cout << "  [control] 龙威(penetrable=false) -> 我方技能无效" << std::endl;
    }

    // 主场景：对手挂"可穿盔"（仅封攻击，penetrable=true）→ 699 凭证穿透 → 我方技能命中
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        // 注入可穿盔：对手(施放方1)封我方(被封方0)攻击技能，可被"无视攻击免疫"穿透
        seal_skill(&battle.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70004,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1, /*duration_rounds=*/0,
                   /*penetrable=*/true);
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT);  // 699 穿透
        std::cout << "  [main] 可穿盔(penetrable=true) + 699 凭证 -> 我方技能命中" << std::endl;
    }

    std::cout << "scenario_d PASS (法咒共鸣斩 699 穿透可穿盔、龙威挡下)"
              << std::endl;
    return 0;
}
