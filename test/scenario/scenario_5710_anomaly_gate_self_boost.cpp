// 场景 5710 —— J240 异常门自身强化族（182，moves_lib/j_anomaly_gate_self_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5710-5719）
//
// 断言口径（精确槽位 + 效果前后快照 + 伤害比值）：
//   - 门 = 对手指定异常槽 end_round 未到期；概率 100% 档直接放行（不消耗随机数）。
//   - T5710.a：13070 混沌冰暴 [182]|[5,4,100,1]，对手冻伤(5) → 自身**速度槽(4)** +1。
//   - T5710.b：同载体，无异常 → 速度槽保持 0。
//   - T5710.c：同载体，对手害怕(6) ≠ {0}=5 → 速度槽保持 0（状态门精确）。
//   - T5710.d：19682 风雪打击 [182]|[5,2,100,2]，对手冻伤(5) → 自身**特攻槽(1)**
//     +2；该强化在 SKILL_EFFECT 写视图、ATTACK_DAMAGE 读视图 → 本刀伤害 ≈2×。
//   - T5710.e：同载体，无异常 → 特攻槽 0，伤害为基线。
//   - 官方码序 {1}=2 → 引擎槽 1（特攻）由 d/e 直证；{1}=4 → 引擎槽 4（速度）由 a 直证。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/battleWorkspace.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;   // 拍打：物 40（填充刀）
constexpr int kSpeedUp  = 13070;   // [182]|[5,4,100,1] 混沌冰暴：特 150
constexpr int kSpaUp    = 19682;   // [182]|[5,2,100,2] 风雪打击：特 130
constexpr int kMaxHp    = 99999;
constexpr int kFrost    = static_cast<int>(AbnormalStatusId::Frostbite);   // 5
constexpr int kFear     = static_cast<int>(AbnormalStatusId::Fear);        // 6

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

struct BoostResult {
    int speed_level = 0;
    int spa_level = 0;
    int damage = 0;
};

// 先手技能效果已结算后读自身等级；再跑到行动后读本次红伤快照。
BoostResult run_case(int carrier, int anomaly_status) {
    ScenarioBattle b(make_party(mk_self(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (anomaly_status >= 0) {
        ctx.apply_abnormal_status_for_rounds(1, anomaly_status, 3);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    BoostResult r;
    r.speed_level = ctx.ability_levels[0][4];   // 4=速度
    r.spa_level = ctx.ability_levels[0][1];     // 1=特攻
    b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
    ctx.debug_step_mode = false;
    r.damage = ctx.resolvedDamage.final;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5710.a/b/c 13070：冻伤门 → 速度 +1 ══
    {
        const BoostResult frost = run_case(kSpeedUp, kFrost);
        std::cout << "  [T5710.a] 13070 frost -> speed level=" << frost.speed_level << std::endl;
        CHECK_EQ(frost.speed_level, 1, "T5710.a 冻伤(5) → 自身速度槽 +1");

        const BoostResult none = run_case(kSpeedUp, -1);
        std::cout << "  [T5710.b] 13070 none -> speed level=" << none.speed_level << std::endl;
        CHECK_EQ(none.speed_level, 0, "T5710.b 无异常 → 速度槽保持 0");

        const BoostResult wrong = run_case(kSpeedUp, kFear);
        std::cout << "  [T5710.c] 13070 fear -> speed level=" << wrong.speed_level << std::endl;
        CHECK_EQ(wrong.speed_level, 0, "T5710.c 害怕(6) ≠ {0}=5 → 速度槽保持 0");
    }

    // ══ T5710.d/e 19682：冻伤门 → 特攻 +2，本刀伤害 ≈2× ══
    {
        const BoostResult frost = run_case(kSpaUp, kFrost);
        const BoostResult none = run_case(kSpaUp, -1);
        std::cout << "  [T5710.d] 19682 frost -> spa level=" << frost.spa_level
                  << " damage=" << frost.damage << " | none spa=" << none.spa_level
                  << " damage=" << none.damage << std::endl;
        CHECK_EQ(frost.spa_level, 2, "T5710.d 冻伤(5) → 自身特攻槽 +2（官方码 2→引擎槽 1）");
        CHECK_EQ(none.spa_level, 0, "T5710.e 无异常 → 特攻槽保持 0");
        CHECK(frost.damage > none.damage * 15 / 10,
              "T5710.d 特攻 +2 在 ATTACK_DAMAGE 前生效 → 本刀伤害 ≈2×（实测 frost=" +
                  std::to_string(frost.damage) + " none=" + std::to_string(none.damage) + "）");
    }

    std::cout << "scenario_5710 PASS (182 异常门自身强化)" << std::endl;
    return 0;
}
