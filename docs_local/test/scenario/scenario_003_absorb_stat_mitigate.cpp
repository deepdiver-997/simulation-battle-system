// 场景 C：希拓·烬灭神咒剑 1287 吸取能力提升 → 下N次受击减伤。
//
// 对手有**速度**能力提升 → 希拓用「烬灭神咒剑」(37218)命中 → 1287
// 「吸取对手能力提升，吸取成功则下N次受到的攻击伤害减少P%」吸取（对手速度等级清零）+
// 注册"下1次受击减伤50%"REDUCE 效果 → 对手后续攻击被减伤。
// 对照组（对手无提升）：1287 不触发 → 对手全力攻击。
//
// ⚠️ 取吸收的 stat 用**速度**而非防御（用户指正）：测量攻击是物理「电光火石」，
//    若吸的是防御，正确语义"吸取把加成加到希拓"会让希拓物防↑ → 物理电光火石伤害↓，
//    就和 REDUCE 的次数减伤混在一起，无法判定减伤来源。改吸**速度**：速度不参与物攻伤害公式，
//    减伤可唯一归因于 REDUCE。"命中"无等级槽位（6 槽=攻/特攻/防/特防/速度/HP，见
//    NumericalPropertyIndex）；速度取 **+1**（+2 会让对手有效速度=100×2=200 与希拓 200 打平
//    → 先手权 rand 掷硬币，测试变随机；+1 有效速度 150 严格落后，希拓确定性先手）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillJinMieShenZhouJian = 37218;  // 烬灭神咒剑（希拓）：2000 条件先制 / 1287 吸取减伤 / 2126 技能无效补偿
constexpr int kSkillDianGuangHuoShi = 10008;     // 电光火石：先制+1，power 40
constexpr int kSkillZhuangJi = 10001;            // 撞击：无效果，priority 0

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 对手血量调大(2000)避免被希拓的烬灭神咒剑直接 KO（否则对手不出手、伤害无法对比）。
    auto pet0 = scenario::make_pet(0, "希拓",
                                   {kSkillJinMieShenZhouJian, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/200, /*atk=*/150, /*hp=*/800);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillDianGuangHuoShi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/300, /*hp=*/2000);

    // 对照组：对手无能力提升 → 1287 不触发 → 对手全力攻击
    int loss_control = 0;
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        loss_control = 800 - battle.ctx().getPet(0).hp;
        std::cout << "  [control] no boost -> 希拓 lost " << loss_control << " hp" << std::endl;
    }

    // 主场景：对手速度能力提升 → 1287 吸取 + 下1次受击减伤50%
    int loss_main = 0;
    // 吸速度（不参与物攻伤害公式）；用 +1 不用 +2（+2 有效速度 200 与希拓平 → 先手 rand 变随机）。
    pet1.levels[static_cast<int>(NumericalPropertyIndex::SPEED)] = 1;
    {
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 对手速度能力提升被 1287 吸取（正等级清零）
        assert(battle.ctx().getPet(1).levels[static_cast<int>(NumericalPropertyIndex::SPEED)] == 0);
        loss_main = 800 - battle.ctx().getPet(0).hp;
        std::cout << "  [main] speed boost drained -> 希拓 lost " << loss_main << " hp" << std::endl;
    }

    // 1287 减伤(50%)使主场景伤害显著低于对照组（对手攻击力相同，仅减伤差异）
    assert(loss_main < loss_control);

    std::cout << "scenario_c PASS (烬灭神咒剑 1287 吸取 + 下1次受击减伤)"
              << std::endl;
    return 0;
}
