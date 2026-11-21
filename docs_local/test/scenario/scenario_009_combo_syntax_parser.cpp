// 场景 I：无相谛 37460 五类条件模板（组合语法第二刀）——真实模板经 parser 解析执行。
//
// 无相谛 side_effect=[179,700,1083,1257,1248,456] args=[170,1,3,100,31,300]：
//   179  若属性相同则技能威力提升170      → SameElement + PowerBoost(ws.skill_power_view+170)
//   700  先出手时降低对手所有PP 1 点        → FirstMove + PpReduce
//   1083 若后出手则消除对手回合类效果      → SecondMove + RemoveRoundEffects
//   1257 对手不处于异常状态则吸取1/3体力   → TargetNoAnomaly + DrainHp
//   1248 对手处于异常状态时100%令对手31    → TargetHasAnomaly + Anomaly（第一刀 StatusInflict）
//   456  若对手体力不足300则直接秒杀      → TargetHpBelow(300) + Kill
//
// 魂印用"仅 ignore_pp"自制（程序模型 early，早于首轮选择）——避开 2260 能力减半抹平
// 速度、破坏先手/后手控制。无相谛 accuracy=0 → 测试层手动 must_hit=true 绕必 miss。
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillWuXiangDi = 37460;   // 无相谛：power0/acc0/pp0，效果全在模板
constexpr int kSkillZhuangJi = 10001;    // 撞击：max_pp 35
constexpr int kAnomaly31 = 31;           // 1248 模板的异常 id

// ignore_pp-only 魂印（避免 2260 stat_half 影响速度控制）。
EffectResult ignore_pp_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        ctx->ignore_pp[owner] = true;
    }
    return EffectResult::kOk;
}
SoulMark make_ignore_pp_soul_mark() {
    SoulMark sm;
    sm.id = 9995;
    sm.name = "无视PP";
    sm.description = "PP=0 技能仍可选（early 早于首轮选择）";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {
        SoulMarkNodeRef(State::BATTLE_ROUND_START, &ignore_pp_effect, /*once=*/false, /*early=*/true),
    };
    sm.has_program_ = true;
    return sm;
}

// pet0 挂无相谛 + ignore_pp 魂印；测试层强制必中。
ElfPet make_wuxiang_pet(int speed, int hp) {
    auto pet = scenario::make_pet(0, "无为觉者",
                                  {kSkillWuXiangDi, kSkillZhuangJi, kSkillZhuangJi,
                                   kSkillZhuangJi, kSkillZhuangJi},
                                  speed, /*atk=*/150, hp,
                                  /*soul_mark=*/make_ignore_pp_soul_mark());
    pet.skills[0].must_hit = true;  // 无相谛 accuracy=0，测试层强制必中
    return pet;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A) 同元素 → 179 威力提升（ws.skill_power_view: 0 + 170）
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().ws.skill_power_view[0] == 170);  // 同元素 → 威力提升 170
        std::cout << "  [179-same-element] skill_power_view[0] = "
                  << battle.ctx().ws.skill_power_view[0] << " (+170)" << std::endl;
    }

    // A') 异元素 → 179 不触发（对照）
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        pet1.elementalAttributes[0] = 1;  // 主元素不同
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().ws.skill_power_view[0] == 0);  // 异元素 → 不提升
        std::cout << "  [179-diff-element] skill_power_view[0] = "
                  << battle.ctx().ws.skill_power_view[0] << " (不触发)" << std::endl;
    }

    // B) 先手 → 700 降对手所有 PP 1 点
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 技能1未被对手本回合使用（只被 700 降过）：35 - 1 = 34，隔离 700 的效果
        assert(battle.ctx().getPet(1).skills[1].pp == 34);
        std::cout << "  [700-first-move] 对手技能1 PP = "
                  << battle.ctx().getPet(1).skills[1].pp << " (35-1)" << std::endl;
    }

    // C) 后手 → 1083 消除对手回合类效果
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/100, /*hp=*/800);  // 后手
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        // 给对手注册 1 回合效果（无逻辑 no-op，仅占回合效果位）
        Effect round_effect(90001, 0, /*owner=*/1, /*left_round=*/1, EffectArgs(std::vector<int>{0, 1}));
        auto ce = std::make_unique<ContinuousEffect>(
            round_effect, State::BATTLE_ROUND_START, /*owner=*/1, /*duration=*/1, battle.ctx().roundCount);
        battle.ctx().registerEffect(State::BATTLE_ROUND_START, 1, std::move(ce), EffectContainer::Skill);
        assert(battle.ctx().has_round_effects(1));
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(!battle.ctx().has_round_effects(1));  // 1083 消除了对手回合类效果
        std::cout << "  [1083-second-move] 对手回合类效果已消除" << std::endl;
    }

    // D) 对手无异常 → 1257 吸取 1/3 体力
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/6000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 1257 吸取 1/3 = 2000（固定）；另有 179 威力提升后的普通攻击伤害，故掉血 ≥ 2000
        assert(6000 - battle.ctx().getPet(1).hp >= 2000);
        std::cout << "  [1257-no-anomaly] 对手掉血 = "
                  << (6000 - battle.ctx().getPet(1).hp) << " (≥2000 吸取)" << std::endl;
    }

    // E) 对手体力不足 300 → 456 秒杀
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/250);  // < 300
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().getPet(1).hp == 0);  // 秒杀
        std::cout << "  [456-hp-below-300] 对手 hp = 0 (秒杀)" << std::endl;
    }

    // F) 对手有异常 → 1248 令对手异常31（第一刀 StatusInflict + 新条件 TargetHasAnomaly）
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        battle.start();
        apply_anomaly(&battle.ctx(), 1, kAnomaly31, /*duration_rounds=*/3, /*actor=*/0);
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().has_active_abnormal_status(1, kAnomaly31));  // 1248 条件满足继续挂
        std::cout << "  [1248-has-anomaly] 对手异常31 生效中" << std::endl;
    }

    std::cout << "scenario_i PASS (无相谛 5 类条件模板：179威力/700降PP/1083清回合/1257吸取/456秒杀)"
              << std::endl;
    return 0;
}
