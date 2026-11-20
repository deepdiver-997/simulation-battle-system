#include <utils/sbs_rand.h>
// 场景 092：死亡裁定冻结——裁定后治疗类效果不得逆转死亡，亡语类照常执行。
//
// 用户口径（2026-10-02）：死亡一旦裁定就是事实，应当马上停下等切换指令；
// 裁定后后续效果统统跳过不执行（亡语类除外）。本场景把这个语义钉成回归：
//
//   先手重锤（side0，攻 10000/速 999）一击打死 冻结宿主（side1，hp 100）。
//   宿主带测试魂印 9995，开局注册两个行为：
//     ① 重置体力：挂 BATTLE_AFTER_DEFEATED 时点桶——死亡登记**之后**仍会执行的
//        通道。执行时先打 kResetAttempted 标记再走 heal_amount（合法原语）。
//        若"裁定后还能奶活"，宿主会在这条通道上站起来。
//     ② 亡语：EVENT_DEATH watcher，死亡时打 kDeathWordFired 标记（亡语类豁免）。
//
//   断言（D2 裁决终局性口径，terminal-dswitch D1-D5）：
//     - kDeathWordFired == 1        亡语照常执行（豁免类）
//     - kResetAttempted == 1        重置通道确实跑到了（证明"跳过"不是"没执行"）
//     - 宿主 hp == 0                重置未生效——治疗无法逆转死亡
//     - state == CHOOSE_AFTER_DEATH 且 player_must_switch_pet(1)——正常等换宠
//     - 换上替补后 on_stage 推进、对局继续
//
// ⚠️ 本场景在 D2 之前的代码上会失败（宿主被奶活 → 等待前提消失）——它就是
//    语义漂移的哨兵：谁改坏裁决终局性，这里先红。

#include "../scenario/scenario_harness.h"

#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

namespace {

// 测试魂印（挂 side1 宿主）：args[0] = 宿主所在方（固定 1）
constexpr int kDoomSoulMarkId = 9995;
constexpr int kResetAttempted = 99100001;  // plugin_storage：重置通道已执行
constexpr int kDeathWordFired = 99100002;  // plugin_storage：亡语已执行

// ① AFTER_DEFEATED 时点桶：重置自身体力（走合法原语 heal_amount）
EffectResult doom_reset_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    ctx->plugin_storage[kResetAttempted] = 1;   // 先证明"通道执行了"
    heal_amount(ctx, owner, pet.numericalBase[NumericalPropertyIndex::HP]);
    return EffectResult::kOk;
}

// ② 魂印入口（登场激活时注册 ①与亡语 watcher）
EffectResult doom_soul_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }

    // 亡语：死亡时打标记（EVENT_DEATH watcher，once）
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/true,
        [owner](BattleContext* c, const BattleEvent&) {
            if (c) {
                c->plugin_storage[kDeathWordFired] = 1;
            }
        },
        WatcherScope::ON_STAGE);

    // 重置体力：挂 AFTER_DEFEATED 桶（死亡登记之后仍会执行的时点）
    Effect wrapper(kDoomSoulMarkId, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner}), &doom_reset_hp);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_AFTER_DEFEATED, owner, /*duration=*/3, ctx->roundCount);
    ce->source_id_ = kDoomSoulMarkId;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_AFTER_DEFEATED, owner, std::move(ce));
    return EffectResult::kOk;
}

SoulMark make_doom_soul_mark() {
    SoulMark sm;
    sm.id = kDoomSoulMarkId;
    sm.name = "冻结宿主魂印";
    sm.description = "测试桩：AFTER_DEFEATED 重置体力（应被死亡裁定冻结）+ 亡语标记（应执行）";
    sm.effect = &doom_soul_effect;
    sm.args = EffectArgs(std::vector<int>{1});
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261002);

    // 先手重锤：速 999 必先手、攻 10000 对 hp100 必杀（伤害浮动 217~255/255 不影响）。
    // 暴击率已被 make_pet 清零（确定性）。
    const auto attacker = scenario::make_pet(9021, "先手重锤",
                                             {36668, 10001, 10001, 10001, 10001},
                                             /*speed=*/999, /*atk=*/10000, /*hp=*/5000);
    // 冻结宿主：hp 100，必被一击打死；替补 hp 100 待命。
    const auto doomed = scenario::make_pet(9022, "冻结宿主",
                                           {10001, 10001, 10001, 10001, 10001},
                                           /*speed=*/1, /*atk=*/10, /*hp=*/100,
                                           make_doom_soul_mark());
    const auto bench = scenario::make_pet(9023, "替补",
                                          {10001, 10001, 10001, 10001, 10001},
                                          /*speed=*/10, /*atk=*/10, /*hp=*/100);
    const std::array<ElfPet, 6> party0{attacker, attacker, attacker,
                                       attacker, attacker, attacker};
    const std::array<ElfPet, 6> party1{doomed, bench, bench, bench, bench, bench};

    scenario::ScenarioBattle battle(party0, party1);
    battle.start();
    CHECK_EQ(battle.state(), State::OPERATION_CHOOSE_SKILL_MEDICAMENT, "开局应到选择期");

    // 回合 1：双方提交，先手重锤一击打死冻结宿主
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();

    // ── 核心断言：死亡裁定冻结 ──
    CHECK_EQ(battle.state(), State::CHOOSE_AFTER_DEATH, "宿主阵亡后应停在死后选择");
    CHECK_EQ(battle.ctx().plugin_storage.count(kDeathWordFired) == 1
                 ? std::any_cast<int>(battle.ctx().plugin_storage[kDeathWordFired]) : 0,
             1, "亡语（EVENT_DEATH watcher）应照常执行（豁免类）");
    CHECK_EQ(battle.ctx().plugin_storage.count(kResetAttempted) == 1
                 ? std::any_cast<int>(battle.ctx().plugin_storage[kResetAttempted]) : 0,
             1, "重置通道（AFTER_DEFEATED 桶）应已执行——冻结不是没跑到");
    CHECK_EQ(battle.pet(1).hp, 0, "宿主必须仍为死亡：治疗不可逆转已裁定的死亡");
    CHECK(battle.ctx().need_input(), "死后选择期应等待输入");
    CHECK_EQ(battle.ctx().seerRobot[1].elfPets[1].hp, 100, "替补不应被波及");

    // ── 换上替补，对局继续 ──
    battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    battle.drive();
    CHECK_EQ(battle.ctx().on_stage[1], 1, "替补应已登场");
    CHECK_EQ(battle.pet(1).hp, 100, "替补满血在场");
    CHECK(battle.ctx().currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT
              || battle.ctx().currentState == State::CHOOSE_AFTER_DEATH
              || battle.ctx().currentState == State::FINISHED,
          "对局应继续推进（不回退、不自旋）");

    return 0;
}
