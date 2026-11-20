// 场景 029：不灭·艾恩斯 秩序之助(31268) —— 消除成功→属性技能"命中失效"（非无效补偿）。
//   秩序之助 side_effect=[781,679]：781 = "消除对手回合类效果，消除成功则{n}回合内令对手属性技能无效"
//   （官方落成**命中效果失效**，SEAL_ATTRIBUTE_HIT：照常命中、效果不注册、不触发 SKILL_INVALID
//   无效补偿，区别于普通封属 SEAL_ATTRIBUTE）。
//
//   被测属性技能=**大雪纷飞(28319)**：含 2006 **无效补偿**（"技能无效时，免疫下1次对手的攻击，
//   免疫成功则令对手全属性+1"）。验证点（要两个都过才算成功）：
//   A) 正常技能效果不触发 —— 大雪纷飞对我方(2004/2005)能力/异常影响未发生（我方等级不变）。
//   B) 无效补偿不触发 —— 大雪纷飞被"命中失效"拦下（非被无效），2006 的 SKILL_INVALID 分支
//      从未走 → EVENT_SKILL_INVALID=0、无"全属性+1"等补偿收益。
//   若这里换成普通封属(SEAL_ATTRIBUTE)，大雪纷飞会走 SKILL_INVALID 触发 2006 —— 两语义分野在此。
//
//   时序：对手超速闪身(28469)挂843回合先制 → 顺序之助消回合成功挂2回合命中失效封属 →
//         大雪纷飞被命中失效（正常效果 + 无效补偿都不触发）。
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <effects/effect_unit.h>
#include <effects/event_center.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {
constexpr int kSkillZhixuZhizhu = 31268;   // 秩序之助：物理85 + 消回合→2回合属性命中失效封属
constexpr int kSkillChaosuShanshen = 28469; // 超速闪身：属性，下4回合自身技能先制+2（回合类效果）
constexpr int kSkillDaxue = 28319;          // 大雪纷飞：属性，自带 2006 无效补偿（免疫下1次攻击+全属性+1）
constexpr int kSkillZhuangji = 10001;       // 撞击：物理，无附加

// 计数两条事件通路：EVENT_HIT（命中） vs EVENT_SKILL_INVALID（无效补偿）——区分"命中失效"与"被无效"。
struct EventCounters {
    int hit = 0;
    int skill_invalid = 0;
};
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(3467, "不灭·艾恩斯",
                                   {kSkillZhixuZhizhu, kSkillZhuangji, kSkillZhuangji,
                                    kSkillZhuangji, kSkillZhuangji},
                                   /*speed=*/300, /*atk=*/150, /*hp=*/3000);
    pet0.skills[0].must_hit = true;  // 顺序之助必中
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillChaosuShanshen, kSkillDaxue, kSkillZhuangji,
                                    kSkillZhuangji, kSkillZhuangji},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/4000);
    pet1.skills[1].must_hit = true;  // 大雪纷飞必中（保证其效果是否被失效可得观测）

    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();

    // 事件计数 watcher（event_center_.deliver 不按 owner 过滤，注册 owner=0 即可抓全部同类事件）。
    EventCounters counters;
    BattleContext& ctx = battle.ctx();
    ctx.event_center_.register_watcher(
        EventType::EVENT_HIT, /*owner=*/0, /*register_round=*/0, /*duration_rounds=*/0,
        /*once=*/false, [&counters](BattleContext*, const BattleEvent&) { ++counters.hit; },
        /*scope=*/WatcherScope::TEAM);  // TEAM：绕开 ON_STAGE 的 valid_id 过滤（跨回合稳定计数）
    ctx.event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, /*owner=*/0, /*register_round=*/0, /*duration_rounds=*/0,
        /*once=*/false, [&counters](BattleContext*, const BattleEvent&) { ++counters.skill_invalid; },
        /*scope=*/WatcherScope::TEAM);

    // 我方(0)全部能力等级清零检查：探测大雪纷飞正常效果/2006补偿("令对手全属性+1")触及我方的任何变化。
    auto assert_my_levels_zero = [&](const char* tag) {
        for (int s = 0; s < 6; ++s) {
            CHECK_EQ(ctx.ability_levels[0][s], 0,
                     std::string(tag) + " 我方能力等级应保持 0（大雪纷飞正常效果/2006补偿均未作用）");
        }
    };

    // ── 回合 1：对手上回合效果（超速闪身 → 843 回合先制），供顺序之助"消除成功"判据。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/1);  // 撞击
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/0);  // 超速闪身
    battle.drive();
    assert_my_levels_zero("回合1后");

    // ── 回合 2：顺序之助消回合成功 → 挂 2 回合命中失效封属；大雪纷飞被命中失效。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/0);  // 顺序之助
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/1);  // 大雪纷飞
    battle.drive();
    assert_my_levels_zero("回合2后");

    // ── 回合 3：命中失效窗口（持续 2 回合：回合2/3）内，大雪纷飞仍被失效。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/1);  // 撞击
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*skill_idx=*/1);  // 大雪纷飞
    battle.drive();
    assert_my_levels_zero("回合3后");

    // ── 关键语义断言：
    // A) 正常效果不触发：大雪纷飞窗口内(回2/3)对内 buff(1687) / 对我方影响(2004/2005) 均未发生——
    //    我方 6 维能力等级全程 0（若触发会触及我方，见 assert_my_levels_zero 已证）。
    // B) 无效补偿不触发：2006 是 SKILL_INVALID 分支，命中失效走 HIT_INVALID 从不走它 → 补偿零触发。
    CHECK_EQ(counters.skill_invalid, 0,
             "B) 大雪纷飞被命中失效(非被无效) → 2006 无效补偿不触发(EVENT_SKILL_INVALID=0)");
    CHECK(counters.hit >= 5, "A) 大雪纷飞照常命中(EVENT_HIT，非被无效)，只是效果失效");

    std::cout << "  [秩序之助·命中失效封属] 大雪纷飞(含2006无效补偿)被命中失效：\n"
              << "    A) 正常效果不触发(我方能力等级 0/0/0/0/0/0) +\n"
              << "    B) 无效补偿不触发(EVENT_SKILL_INVALID=0，2006全属性+1未发生) → 非无效补偿语义\n";
    std::cout << "scenario_029 PASS (大雪纷飞：命中失效下 正常效果与无效补偿皆不触发)\n";
    return 0;
}