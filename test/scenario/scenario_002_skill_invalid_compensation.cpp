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
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    // 对手能力提升（验证 2126 消除）：等级本体在 context，**开战后**写入（on-stage 作用域）
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        battle.ctx().ability_levels[1][i] = 2;
    }
    // 注入龙威：对手(施放方1)封希拓(被封方0)的攻击+属性技能，不可穿透
    seal_skill(&battle.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70001,
               /*attribute=*/true, /*attack=*/true, /*count=*/1, /*duration_rounds=*/0, /*penetrable=*/false);
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();

    // 希拓技能被龙威 SEALED → SKILL_INVALID
    REQUIRE(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);
    // 2126 补偿：对手焚烬 + 能力提升被消除。
    // ⚠️ 2026-09-20 修正：旧实现把"焚烬"错写成烧伤槽（id=2）直写；现走 apply_anomaly
    //    真原语施加 **Incinerate(22)**（现代通道，免疫/抗性全检查）。
    //    （2026-09-22 合并 fix/o3-ub：assert → REQUIRE，NDEBUG 下恒开。）
    constexpr int kIncinerateAnomalyId = 22;  // AbnormalStatusId::Incinerate（焚烬）
    REQUIRE(battle.ctx().has_active_abnormal_status(1, kIncinerateAnomalyId));
    REQUIRE(battle.ctx().ability_levels[1][0] == 0);

    std::cout << "scenario_b PASS (烬灭神咒剑 2126 技能无效补偿：焚烬+消除能力提升)"
              << std::endl;
    return 0;
}
