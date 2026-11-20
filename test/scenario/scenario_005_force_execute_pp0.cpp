// 场景 E：强制执行凭证——PP=0 技能必定命中（绕过高闪避 miss 率）。
//
// 注入无为觉者 2260 魂印的信号（force_execute_on_pp0 + ignore_pp，对应"使用 PP=0 技能时
// 必定命中、强制执行命中效果"），对手挂高闪避魂印（闪避率 100%，放大我方 miss 率）。
// 对照组：无信号、普通技能 → 被高闪避闪掉（SKILL_INVALID）。
// 主场景：PP=0 + 信号 → materialize 得 force_execute → 必定命中（HIT）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillZhuangJi = 10001;  // 撞击：无效果，priority 0，accuracy 95

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 对照组：无强制执行信号、普通技能（PP>0），对手高闪避 → 我方技能被闪掉
    {
        auto pet0 = scenario::make_pet(0, "我方",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "闪避对手",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/1000,
                                       /*soul_mark=*/scenario::make_dodge_soul_mark());
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        REQUIRE(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);  // 被高闪避闪掉
        std::cout << "  [control] 高闪避 -> 我方技能 miss (SKILL_INVALID)" << std::endl;
    }

    // 主场景：真实 2260 魂印 + PP=0 技能 → 必定命中。
    // 2260 在战斗开始（OPERATION_ENTER_EXIT_STAGE）由 activate_soul_mark 激活，
    // 立即设置 ignore_pp + force_execute_on_pp0 → PP=0 技能可选且必定命中。
    {
        auto pet0 = scenario::make_pet(0, "无为觉者",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
        pet0.skills[0].pp = 0;  // 使用 PP=0 技能（触发 2260 强制）
        auto pet1 = scenario::make_pet(1, "闪避对手",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/1000,
                                       /*soul_mark=*/scenario::make_dodge_soul_mark());
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        REQUIRE(battle.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT);  // 必定命中
        std::cout << "  [main] 真实2260 + PP=0 -> 必定命中 (HIT)" << std::endl;
    }

    std::cout << "scenario_e PASS (强制执行凭证：PP=0 技能必定命中绕过高闪避)"
              << std::endl;
    return 0;
}
