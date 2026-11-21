// 场景 G：无为觉者 2260 完整子句（程序模型多时点）。
//
// 2260 真实文本（effect_icon）的子句按程序节点拆到不同时点：
//   ① 能力减半（战斗开始 ENTER_EXIT_STAGE）：六项能力均 = 总和/2
//   ② 不受PP + PP反转（ROUND_START 信号，early 早于首轮技能选择）：
//      使用技能后 PP = maxPP - 原PP（当前与已损失互换）
//   ③ PP≠0 技能命中 → 解除所处异常 + 免疫下次异常（ON_SKILL_HIT）
//   PP=0 强制已由场景 E 覆盖；万相乖离/无为依赖 effect-unit 条件取消机制，留 TODO。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillDianGuangHuoShi = 10008;  // 电光火石：先制+1，accuracy 100，max_pp 30
constexpr int kSkillZhuangJi = 10001;         // 撞击：无效果，priority 0，accuracy 95
constexpr int kBurnStatusId = 2;              // AbnormalStatusId::Burn（烧伤）
constexpr int kHalfStat = 750;  // make_pet 六项能力 150+150+100+100+200+800=1500 → 总和/2

// 构造对手：高血量（避免被 2260 能力减半后的高攻击 KO），低速度（我方先手）。
ElfPet make_opponent() {
    return scenario::make_pet(1, "对手",
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/2000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ① 能力减半：战斗开始（ENTER_EXIT_STAGE）后六项能力均 == 总和/2
    {
        auto pet0 = scenario::make_pet(0, "无为觉者",
                                       {kSkillDianGuangHuoShi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(make_opponent()));
        battle.start();  // 已驱动到 CHOOSE；ENTER_EXIT_STAGE 的能力减半已执行
        const auto& p = battle.ctx().getPet(0);
        for (int i = 0; i <= static_cast<int>(NumericalPropertyIndex::HP); ++i) {
            assert(p.numericalProperties[static_cast<NumericalPropertyIndex>(i)] == kHalfStat);
        }
        std::cout << "  [stat-half] 六项能力 -> " << kHalfStat << " (总和/2)" << std::endl;
    }

    // ② PP 反转：技能 PP=3（max 30），使用后 PP = 30 - 3 = 27（当前与已损失互换）
    {
        auto pet0 = scenario::make_pet(0, "无为觉者",
                                       {kSkillDianGuangHuoShi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
        pet0.skills[0].pp = 3;  // 电光火石 max_pp 30
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(make_opponent()));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().getPet(0).skills[0].pp == 27);  // maxPP(30) - 原PP(3)
        std::cout << "  [pp-reverse] PP 3 -> " << battle.ctx().getPet(0).skills[0].pp
                  << " (max 30 - 原3)" << std::endl;
    }

    // ③ PP≠0 技能命中：解除所处异常（烧伤）+ 免疫下次异常
    {
        auto pet0 = scenario::make_pet(0, "无为觉者",
                                       {kSkillDianGuangHuoShi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(make_opponent()));
        battle.start();
        // 给 pet0 施加 3 回合烧伤（actor=1 敌方）
        apply_anomaly(&battle.ctx(), 0, kBurnStatusId, /*duration_rounds=*/3, /*actor=*/1);
        assert(battle.ctx().has_active_abnormal_status(0, kBurnStatusId));
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 电光火石（PP≠0）命中 → 解除烧伤 + 授予"免疫下次异常"
        assert(!battle.ctx().has_active_abnormal_status(0, kBurnStatusId));
        assert(battle.ctx().is_immune(0, ImmunityType::ANOMALY,
                                      State::BATTLE_FIRST_ON_SKILL_HIT, kBurnStatusId));
        std::cout << "  [on-skill-hit] PP!=0 -> 烧伤已解除 + 免疫下次异常" << std::endl;
    }

    std::cout << "scenario_g PASS (2260 完整子句：能力减半 / PP反转 / PP!=0 解异常+免疫)"
              << std::endl;
    return 0;
}
