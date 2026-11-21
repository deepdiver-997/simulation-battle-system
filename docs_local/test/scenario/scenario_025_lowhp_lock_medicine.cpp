// 场景 025：不灭·艾恩斯 1067 —— 锁1血精确检测 + 嗑药即时生效。
//   艾恩斯嗑 **PP药**（不是 HP药）→ 不补血也不攻击; 对手打死它一条命 → 锁血后**恰好只剩 1 血**可干净检测。
//   对手 hp=1（体力不高于我 → ③低体回血不触发），并给对手**魂免烧伤**（防被①的 2回合烧伤烫死, 让回合跑完）。
//   另验药品**即时生效**: PP药在操作提交那一刻(USE_MEDICINE action)就恢复, 不等精灵行动回合
//     （艾恩斯这回合选了药品、没出招, PP 却已恢复 → 证实即时而非延迟到"本会挪出的行动"）。
//
//   R1 对手超高速超攻一击致命(打死一条命) → 艾恩斯锁到 1:
//        PP药不补血 + 对手不高于我 → ③不回血 → hp 恰=1; PP 已被药品即时恢复; 对手活(魂免+未被攻击)
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;      // 撞击（物理攻击）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);  // 2

ElfPet make_aigens(int hp) {
    return scenario::make_pet(3467, "不灭·艾恩斯",
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/hp,
                              /*soul_mark=*/SoulMark(1067, "不灭·艾恩斯", "不死锁血",
                                                      EffectArgs(std::vector<int>{0, 1})));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = make_aigens(/*hp=*/100);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/900, /*atk=*/5000, /*hp=*/1);  // 超高速超攻一击必杀, 对手hp=1
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    BattleContext& ctx = battle.ctx();

    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/1u << kBurn,
                       /*duration_rounds=*/0, /*source_id=*/9001);  // 对手魂免烧伤, 不被①烫死, 让回合跑完
    ctx.seerRobot[0].medicines[1] = 5;                // 药槽1 = 恢复PP药(MedicineType::RESTORE_PP)
    ctx.getPet(0).skills[0].pp = 1;                   // 压低 PP, 观测药品即时恢复(1 -> 1+kMedicineRestorePP)
    const int pp_before = ctx.getPet(0).skills[0].pp;

    battle.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 1);  // 艾恩斯嗑 PP药(不攻击不补血)
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // 对手撞击(先手超攻, 打死一条命)
    battle.drive();

    // 锁血: hp100 被一击致命 → 只剩 1 血（PP药没补血 + 对手(1)不高于我 → ③不回血 → 恰=1）
    assert(ctx.getPet(0).hp == 1);
    // 药品即时生效: PP药在操作提交那一刻恢复, 不等"艾恩斯行动回合"(本回合艾恩斯没出招)
    assert(ctx.getPet(0).skills[0].pp > pp_before);
    // 对手活: 艾恩斯用药品未攻击 + 魂免未被烫死
    assert(ctx.getPet(1).hp == 1);
    std::cout << "  [R1] 艾恩斯被一击致命→锁剩 hp=" << ctx.getPet(0).hp
              << " (PP药未补血+对手不高于我→③未回血); PP " << pp_before << "->"
              << ctx.getPet(0).skills[0].pp << " (药品即时生效); 对手魂免+未被攻击→仍hp="
              << ctx.getPet(1).hp << std::endl;

    std::cout << "scenario_025 PASS (锁1血精确检测: PP药不补血+对手不高超我=恰剩1血; + 嗑药即时生效)"
              << std::endl;
    return 0;
}