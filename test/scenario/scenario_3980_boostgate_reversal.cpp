// 场景 3980 —— K248 对手提升门攻击反转族（812，soul_lib/k248_boostgate_reversal.cpp）
//（2026-10-13，worktree ../agents/k248；号段 3980-3989 → T3980-T3982 三块）
//
// 断言口径（db[0]=动词位：1=反转 / 0=消除）：
//   · 对手预置攻击 +2（开战后写 ability_levels）：
//     R1 门开（对手有提升）→ 印主攻击 → 反转模式：对手攻击槽变 -2；消除模式：归 0；
//   · R2 负控：对手无提升 → 门闭，攻击不动其等级；
//   · 印主速度 300 先手；双方 NONE/攻击不影响等级断言面。
//   ⚠️ 纯 drive() 流（3720 记档）。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;
constexpr int kMaxHp = 99999;
constexpr int kAtkSlot = 0;

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(812, "K248 魂印", "",
                                               EffectArgs(row_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void mark_attacks(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3980 反转模式（row508 [1,0]）══
    {
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;
        mark_attacks(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                 "T3980 812：反转模式 → 对手攻击 +2 变 -2");
        mark_attacks(b);   // R2：对手已无正提升 → 门闭，等级不再变
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                 "T3980 812：门闭负控（-2 不再变化）");
        std::cout << "  [T3980] 反转模式 通过\n";
    }

    // ══ T3981 消除模式（row509 [0,0]）══
    {
        ScenarioBattle b(make_party(mark_pet({0, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;
        mark_attacks(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T3981 812：消除模式 → 对手攻击强化归零");
        std::cout << "  [T3981] 消除模式 通过\n";
    }

    // ══ T3982 门闭负控：对手无提升 → 攻击不动等级 ══
    {
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        mark_attacks(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T3982 812：对手无提升 → 门闭不动");
        std::cout << "  [T3982] 门闭负控 通过\n";
    }

    std::cout << "scenario_3980 PASS (K248 对手提升门攻击反转族)" << std::endl;
    return 0;
}
