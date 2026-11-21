// 场景 B：希拓·烬灭神咒剑 技能无效补偿（effect 2126）。
//
// 对手挂"龙威"（封攻+封属、不可穿透）→ 希拓用「烬灭神咒剑」(37218)被 SEALED →
// SKILL_INVALID 分支 → 2126「技能无效时，消除对手回合类效果、能力提升效果，令对手焚烬」。
// 注入：对手能力提升（验证消除）、seal_skill 龙威（penetrable=false）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillJinMieShenZhouJian = 37218;  // 烬灭神咒剑（希拓）：2000 条件先制 / 1287 吸取减伤 / 2126 技能无效补偿
constexpr int kSkillZhuangJi = 10001;             // 撞击：无效果，priority 0

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(0, "希拓",
                                   {kSkillJinMieShenZhouJian, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/200, /*atk=*/250, /*hp=*/500);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/200, /*hp=*/500);
    for (int i = 0; i < 6; ++i) {
        pet1.levels[i] = 2;  // 对手能力提升（验证 2126 消除）
    }

    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    // 注入龙威：对手(施放方1)封希拓(被封方0)的攻击+属性技能，不可穿透
    seal_skill(&battle.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70001,
               /*attribute=*/true, /*attack=*/true, /*count=*/1, /*duration_rounds=*/0, /*penetrable=*/false);
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();

    // 希拓技能被龙威 SEALED → SKILL_INVALID
    assert(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);
    // 2126 补偿：对手焚烬（烧伤异常 id=2）+ 能力提升被消除
    constexpr int kBurnAnomalyId = 2;  // AbnormalStatusId::Burn（烧伤）
    assert(battle.ctx().has_active_abnormal_status(1, kBurnAnomalyId));
    assert(battle.ctx().getPet(1).levels[0] == 0);

    std::cout << "scenario_b PASS (烬灭神咒剑 2126 技能无效补偿：焚烬+消除能力提升)"
              << std::endl;
    return 0;
}
