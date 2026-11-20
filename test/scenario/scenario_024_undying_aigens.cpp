// 场景 024：不灭·艾恩斯 1067 —— 三条子句（全部走 core 真原语，魂印从库组装）。
//   ① 对手出手回合受致命伤 → 残留1体力 + 消除双方能力提升和回合类 + 令对手2回合烧伤（每场1次）
//   ② 回合开始若体力>对手 → 当回合受攻击使对手焚烬；未触发焚烬 → 消除对手回合类
//   ③ 战斗阶段结束若体力<对手 → 恢复自身已损失体力的1/2
// 验证（魂印用真实 1067, 非孤立效果；①②boss无效在 PvP 无 boss, 跳过）：
//   A 锁血: 低血被一击致命 → 不死(hp>0, 否则为 0) + 提升被清 + 对手中烧伤(2回合)
//   B 高体焚烬: hp高于对手且本回合受击 → 对手中焚烬
//   C 未焚断回合: hp高于对手但本回合不受击 → 回合结束断对手回合类
//   D 低体回血: hp低于对手且掉血 → 回合结束回已损1/2（对照：无魂印不回）
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;      // 撞击（物理攻击）
constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动（属性技能，特攻 0）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate); // 22

ElfPet make_aigens(int hp, bool with_soul) {
    SoulMark sm =
        with_soul ? SoulMark(1067, "不灭·艾恩斯", "不死锁血", EffectArgs(std::vector<int>{0, 1}))
                  : SoulMark{};  // 对照组：无魂印
    return scenario::make_pet(3467, "不灭·艾恩斯",
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/hp, /*soul_mark=*/sm);
}

// 给 0 号(艾恩斯)挂攻击提升 + 双方各一个回合类占位。用于 A 验证"消除"。
void setup_round1(BattleContext& ctx) {
    ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] = 2;
    for (int p = 0; p < 2; ++p) {
        Effect round_effect(90002, 0, /*owner=*/p, /*left_round=*/1, EffectArgs(std::vector<int>{0}));
        auto ce = std::make_unique<ContinuousEffect>(
            round_effect, State::BATTLE_ROUND_START, /*owner=*/p, /*duration=*/1, ctx.roundCount);
        ctx.registerEffect(State::BATTLE_ROUND_START, p, std::move(ce), EffectContainer::Skill);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ---- A) 锁血：艾恩斯低血被一击致命 → 不死 + 清提升 + 对手烧伤 ----
    {
        auto pet0 = make_aigens(/*hp=*/100, /*with_soul=*/true);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                        /*speed=*/400, /*atk=*/500, /*hp=*/5000);  // 高速高攻一击必杀
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        BattleContext& ctx = battle.ctx();
        setup_round1(ctx);
        REQUIRE(ctx.has_round_effects(0) && ctx.has_round_effects(1));
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // ① 锁血：hp 100 被一击(≥100)必死，但魂印不死 → hp>0（③低体回血还会把它回上去, 故 ≠1 也合法）
        REQUIRE(ctx.getPet(0).hp > 0);
        // ① 消除双方提升(艾恩斯攻击+2→0) + 断双方回合类
        REQUIRE(ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] == 0);
        REQUIRE(!ctx.has_round_effects(0) && !ctx.has_round_effects(1));
        // ① 令对手 2 回合烧伤
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [A 锁血] hp100被一击致命 -> 不死 hp=" << ctx.getPet(0).hp
                  << ", 攻击提升清 2->" << ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)]
                  << ", 双方回合类断, 对手烧伤✓" << std::endl;
    }

    // ---- B) 高体受击 → 对手焚烬 ----
    {
        auto pet0 = make_aigens(/*hp=*/2000, /*with_soul=*/true);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                        /*speed=*/400, /*atk=*/500, /*hp=*/1000);  // 艾恩斯 hp>对手(②触发)
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        REQUIRE(battle.ctx().has_active_abnormal_status(1, kIncinerate));  // 本回合受击 → 对手焚烬
        std::cout << "  [B 高体焚烬] hp2000>1000 本回合受击 -> 对手焚烬✓ (艾恩斯 hp="
                  << battle.ctx().getPet(0).hp << ")" << std::endl;
    }

    // ---- C) 高体但本回合不受击 → 未焚烬 → 断对手回合 ----
    {
        auto pet0 = make_aigens(/*hp=*/2000, /*with_soul=*/true);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                        /*speed=*/400, /*atk=*/500, /*hp=*/1000);  // 用属性技能: 不攻击艾恩斯
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        BattleContext& ctx = battle.ctx();
        // 给对手挂一个回合类占位, 验证"未焚烬→断对手回合"把它消除
        Effect round_effect(90002, 0, /*owner=*/1, /*left_round=*/1, EffectArgs(std::vector<int>{0}));
        auto ce = std::make_unique<ContinuousEffect>(
            round_effect, State::BATTLE_ROUND_START, /*owner=*/1, /*duration=*/1, ctx.roundCount);
        ctx.registerEffect(State::BATTLE_ROUND_START, 1, std::move(ce), EffectContainer::Skill);
        REQUIRE(ctx.has_round_effects(1));
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 对手本回合没打艾恩斯 → 焚烬未触发 → 回合结束断对手回合类
        REQUIRE(!battle.ctx().has_round_effects(1));
        REQUIRE(!battle.ctx().has_active_abnormal_status(1, kIncinerate));
        std::cout << "  [C 未焚断回合] 高体但对手不耗 → 未焚烬, 对手回合类被断✓" << std::endl;
    }

    // ---- D) 低体回合结束 → 回已损1/2（对照无魂印）----
    {
        int with_soul_hp_end = 0, no_soul_hp_end = 0;
        for (bool with_soul : {true, false}) {
            auto pet0 = make_aigens(/*hp=*/1000, with_soul);
            auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                        kSkillZhuangJi, kSkillZhuangJi},
                                            /*speed=*/400, /*atk=*/500, /*hp=*/5000);
            scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
            battle.start();
            battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
            (with_soul ? with_soul_hp_end : no_soul_hp_end) = battle.ctx().getPet(0).hp;
        }
        // 有魂印: 掉血后低体 → 回合结束回已损1/2 → hp 高于无魂印
        REQUIRE(with_soul_hp_end > no_soul_hp_end);
        std::cout << "  [D 低体回血] 无魂印末hp=" << no_soul_hp_end
                  << " -> 有魂印末hp=" << with_soul_hp_end << " (③回已损1/2见效)" << std::endl;
    }

    std::cout << "scenario_024 PASS (不灭·艾恩斯1067 三子句：锁血+清双方+烧伤/高体焚烬·未焚断回合/低体回血)"
              << std::endl;
    return 0;
}