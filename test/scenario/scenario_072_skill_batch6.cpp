#include <utils/sbs_rand.h>
// 场景 072：技能批 6 —— 全属性条件翻倍余量 + 消回合成功族（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）+ 图鉴原文核对：
//   - 893 全属性+{0}，自身**满体力**时强化效果翻倍（渡世渡劫 24736 词条）
//   - 911 全属性+{0}，自身**处于能力提升状态**时强化效果翻倍（气压万军 24832 词条）
//   - 1283 全属性+{0}，**先出手**时强化效果翻倍（余音袅袅 25896 词条）
//   - 797 消除对手回合类效果，消除成功{0}回合内对手无法通过自身技能恢复体力
//         （魂·罪恶叹息 30815「消除成功则对手2回合内无法通过技能恢复体力」）
//   - 889 消除对手回合类效果，消除成功则令对手{0}（{0}=battle_effects 异常码；
//         王·异甲同心 args=7→疲惫、炫光云空决 args=0→麻痹，图鉴原文逐条对上）
//
// 验证点：
//   A1 893：满体力 → 全属性+2；预先压血 → 全属性+1
//   A2 911：无提升状态 → +1；预置 +1 攻击 → +2
//   A3 1283：先手 → +2；后手 → +1
//   B1 797：守方有回合类效果 → 清掉 + 守方回血被封；无回合类效果 → 不清不封
//   B2 889：守方有回合类效果 → 清掉 + 守方中异常；无回合类效果 → 不中异常
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40
constexpr int kGaoSu = 20017;      // 高速移动：属性技（无伤害无还手）
constexpr int k893 = 24736;        // 渡世渡劫：[893,765,191] args[1,…]，属性、必中
constexpr int k911 = 29064;        // 归真复元：[911] args[1]，属性、必中
constexpr int k1283 = 25896;       // 余音袅袅：[1283,532,843] args[1,…]，属性、必中
constexpr int k797 = 18171;        // 电磁辐射：[797] args[2]，物攻 80、acc 100
constexpr int k889 = 19281;        // 帝天圣战：[889] args[6]，物攻 80、acc 100（6=害怕）

const int kFear = abnormal_status_id(AbnormalStatusId::Fear);

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

ElfPet attacker(int skill, int speed = 200) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill}, speed, 200, 4000);
}
ElfPet quiet_defender(int speed = 100) {
    return scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, 100, 4000);
}

bool all_levels_eq(const BattleContext& ctx, int side, int v) {
    for (int i = 0; i < 5; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

// 给 owner 预挂一条"回合类效果"（ROUND_START 桶、存活 3 回合），供 797/889 的断回合子句消费。
EffectResult marker_effect(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    return EffectResult::kOk;
}
void register_round_marker(BattleContext& ctx, int owner) {
    Effect e(999072, 0, owner, 3, EffectArgs(std::vector<int>{owner, 1 - owner}), &marker_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                 /*duration=*/3, ctx.roundCount);
    ce->source_id_ = 999072;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A1 effect 893：满体力翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k893), quiet_defender());
        b.start();
        round_of(b);   // 满血 → 翻倍
        CHECK(all_levels_eq(b.ctx(), 0, 2), "A1) 满体力 → 全属性+2");
        std::cout << "A1a PASS 893 满体力翻倍 +2\n";

        ScenarioBattle b2 = make_battle(attacker(k893), quiet_defender());
        b2.start();
        b2.pet(0).hp = 2000;   // 非满血
        round_of(b2);
        CHECK(all_levels_eq(b2.ctx(), 0, 1), "A1) 非满体力 → 全属性+1");
        std::cout << "A1b PASS 893 非满体力 +1\n";
    }

    //========== A2 effect 911：处于能力提升状态时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k911), quiet_defender());
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 1), "A2) 无提升状态 → 全属性+1");
        std::cout << "A2a PASS 911 无提升 +1\n";

        ScenarioBattle b2 = make_battle(attacker(k911), quiet_defender());
        b2.start();
        b2.ctx().ability_levels[0][0] = 1;   // 预置 +1 攻击 = "处于能力提升状态"
        b2.ctx().ws.view_levels[0][0] = 1;
        round_of(b2);
        CHECK_EQ(b2.ctx().ability_levels[0][0], 3, "A2) 有提升 → 翻倍（攻击 1+2=3）");
        CHECK_EQ(b2.ctx().ability_levels[0][1], 2, "A2) 其余属性也翻倍 +2");
        std::cout << "A2b PASS 911 有提升翻倍 +2\n";
    }

    //========== A3 effect 1283：先出手时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k1283, /*speed=*/200), quiet_defender(/*speed=*/100));
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 2), "A3) 先出手 → 全属性+2");
        std::cout << "A3a PASS 1283 先手翻倍 +2\n";

        ScenarioBattle b2 = make_battle(attacker(k1283, /*speed=*/50), quiet_defender(/*speed=*/100));
        b2.start();
        round_of(b2);
        CHECK(all_levels_eq(b2.ctx(), 0, 1), "A3) 后出手 → 全属性+1");
        std::cout << "A3b PASS 1283 后手 +1\n";
    }

    //========== B1 effect 797：消回合成功 → 对手封回血 ==========
    {
        // 有回合类效果：被清 + 对手（守方）回血被封
        ScenarioBattle b = make_battle(attacker(k797), quiet_defender());
        b.start();
        register_round_marker(b.ctx(), /*owner=*/1);
        CHECK(b.ctx().has_round_effects(1), "B1) 前提：守方已有回合类效果");
        round_of(b);
        CHECK(!b.ctx().has_round_effects(1), "B1) 消回合成功后守方回合类效果应被清");
        b.pet(1).hp = 1000;   // 压低避免治疗撞上限
        (void)heal_amount(&b.ctx(), 1, 500);
        CHECK_EQ(b.pet(1).hp, 1000, "B1) 守方回血应被封（HEAL_BLOCK）");
        std::cout << "B1a PASS 797 消回合成功 → 封回血\n";

        // 无回合类效果：子句不触发 → 回血正常
        ScenarioBattle b2 = make_battle(attacker(k797), quiet_defender());
        b2.start();
        CHECK(!b2.ctx().has_round_effects(1), "B1) 前提：守方无回合类效果");
        round_of(b2);
        b2.pet(1).hp = 1000;
        (void)heal_amount(&b2.ctx(), 1, 500);
        CHECK_EQ(b2.pet(1).hp, 1500, "B1) 无回合类效果 → 回血正常（1000→1500）");
        std::cout << "B1b PASS 797 无回合类效果不封\n";
    }

    //========== B2 effect 889：消回合成功 → 令对手异常 ==========
    {
        ScenarioBattle b = make_battle(attacker(k889), quiet_defender());
        b.start();
        register_round_marker(b.ctx(), 1);
        round_of(b);
        CHECK(!b.ctx().has_round_effects(1), "B2) 消回合成功后守方回合类效果应被清");
        CHECK(b.ctx().has_active_abnormal_status(1, kFear), "B2) 守方应中害怕（args[0]=6）");
        std::cout << "B2a PASS 889 消回合成功 → 令对手害怕\n";

        ScenarioBattle b2 = make_battle(attacker(k889), quiet_defender());
        b2.start();
        round_of(b2);   // 守方无回合类效果 → 不给异常
        CHECK(!b2.ctx().has_active_abnormal_status(1, kFear), "B2) 无回合类效果 → 不中异常");
        std::cout << "B2b PASS 889 无回合类效果不发异常\n";
    }

    std::cout << "\n场景 072 全部通过：技能批 6（893/911/1283/797/889）✓" << std::endl;
    return 0;
}
