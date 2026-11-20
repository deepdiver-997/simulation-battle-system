// 场景 5760 —— J245 低体回复族（1093，moves_lib/j_low_hp_heal_fraction.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5760-5769）
//
// 断言口径（绝对 hp 差 + 满血钳制）：
//   - 载体 35382 异兽电流 [1093]|[3]（特 140，纯链）；最大体力 9000 → 恢复量 3000。
//   - T5760.a：自身 3000 < 对手 9000 → 恢复 max/3=3000 → 6000。
//   - T5760.b：自身 9000 == 对手 9000 → 不触发（严格 <）。
//   - T5760.c：自身 9000 > 对手 5000 → 不触发。
//   - T5760.d：自身 5000 == 对手 5000 → 不触发。
//   - T5760.e：自身 8000 < 对手 9000 → 恢复 3000 被满血钳到 9000（clamp 口径）。
//   - 观测面 = 先手 SKILL_EFFECT 结算后（对手后续攻击尚未发生），读 ctx.getPet(0).hp。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物 40（填充刀）
constexpr int kHeal   = 35382;   // [1093]|[3] 异兽电流：特 140（纯链）
constexpr int kMaxHp  = 9000;

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kHeal, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 自身/对手当前体力设定后出手，停在先手 SKILL_EFFECT 结算后。
int self_hp_after_cast(int self_hp, int foe_hp) {
    ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = self_hp;
    ctx.getPet(1).hp = foe_hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    ctx.debug_step_mode = false;
    return ctx.getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const int a = self_hp_after_cast(3000, 9000);
    std::cout << "  [T5760.a] self 3000 < foe 9000 -> hp=" << a << std::endl;
    CHECK_EQ(a, 6000, "T5760.a 低体门 → 恢复 max/3=3000 → 6000");

    const int b_equal = self_hp_after_cast(9000, 9000);
    std::cout << "  [T5760.b] self 9000 == foe 9000 -> hp=" << b_equal << std::endl;
    CHECK_EQ(b_equal, 9000, "T5760.b 相等不触发（严格 <）");

    const int c_high = self_hp_after_cast(9000, 5000);
    std::cout << "  [T5760.c] self 9000 > foe 5000 -> hp=" << c_high << std::endl;
    CHECK_EQ(c_high, 9000, "T5760.c 自身高体不触发");

    const int d_equal = self_hp_after_cast(5000, 5000);
    std::cout << "  [T5760.d] self 5000 == foe 5000 -> hp=" << d_equal << std::endl;
    CHECK_EQ(d_equal, 5000, "T5760.d 相等不触发（严格 <）");

    const int e_clamp = self_hp_after_cast(8000, 9000);
    std::cout << "  [T5760.e] self 8000 < foe 9000 -> hp=" << e_clamp << std::endl;
    CHECK_EQ(e_clamp, 9000, "T5760.e 恢复 3000 被满血钳到 9000");

    std::cout << "scenario_5760 PASS (1093 低体回复 max/{0})" << std::endl;
    return 0;
}
