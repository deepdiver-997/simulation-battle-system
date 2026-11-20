// 场景 5630 —— J232 回合消强窗族（1042，moves_lib/j_purge_boosts_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5630-5639）
//
// 断言口径（逐槽绝对值 + 逐回合快照；不写"等级和"）：
//   - 1042 模板：{0}回合内每回合自身技能消除对手能力提升状态。
//   - a) 26526 无光之音 [3]：R1 施放当回合立即消除（对手攻击 +2/防御 +1 → 全 0）。
//   - b) R2/R3：每回合重设对手攻击 +2 → ROUND_START tick 各消除一次（0）。
//   - c) R4：窗过期（R1-R3）→ 重设 +2 不再消除，保持 +2。
//   - d) 对照组：拍打（无 1042）→ 对手 +2 保持。
//   - e) 28180 无念冰容 [4]：R1 立即 + R2/R3/R4 逐回合消除；R5 过期保持 +2。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充/对照刀）
constexpr int kWuGuang = 26526;  // 无光之音 [1042]|[3]（属性技，纯链）
constexpr int kWuNian  = 28180;  // 无念冰容 [1042]|[4]（属性技，纯链）
constexpr int kAtkSlot = 0;      // 0=攻击
constexpr int kDefSlot = 2;      // 2=防御（旁证槽）
constexpr int kMaxHp   = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill0) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手攻击置为 +2（按当前值求增量，便于逐回合重设）。
void set_foe_atk2(BattleContext& ctx) {
    const int cur = ctx.ability_levels[1][kAtkSlot];
    (void)stat_change(&ctx, 1, kAtkSlot, 2 - cur);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5630.a-d 26526 [3]：立即 + 逐回合 + 过期 + 对照 ══
    {
        ScenarioBattle b(make_party(mk_self(kWuGuang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, kAtkSlot, 2) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 1, kDefSlot, 1) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T5630.a R1 当回合立即消除攻击 +2（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0,
                 "T5630.a R1 当回合立即消除防御 +1（实测 " +
                     std::to_string(ctx.ability_levels[1][kDefSlot]) + "）");

        set_foe_atk2(ctx);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2, "T5630.b R2 前置：对手攻击 +2");
        idle_round(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T5630.b R2 tick 消除（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");

        set_foe_atk2(ctx);
        idle_round(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T5630.b R3 窗末回合仍消除（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");

        set_foe_atk2(ctx);
        idle_round(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2,
                 "T5630.c R4 窗过期 → 保持 +2（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        std::cout << "  [T5630.a-c] 3 回合窗立即/逐回合/过期通过\n";
    }

    // ══ T5630.d 对照：拍打无 1042 → 对手 +2 保持 ══
    {
        ScenarioBattle b(make_party(mk_self(kPaiDa)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, kAtkSlot, 2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2,
                 "T5630.d 对照：无 1042 → 攻击 +2 保持（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        std::cout << "  [T5630.d] 对照组通过\n";
    }

    // ══ T5630.e 28180 [4]：4 回合窗 → R4 仍消除，R5 过期 ══
    {
        ScenarioBattle b(make_party(mk_self(kWuNian)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, kAtkSlot, 2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T5630.e R1 立即消除");
        for (int r = 2; r <= 4; ++r) {
            set_foe_atk2(ctx);
            idle_round(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                     "T5630.e R" + std::to_string(r) + " 4 回合窗内仍消除（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        }
        set_foe_atk2(ctx);
        idle_round(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 2,
                 "T5630.e R5 窗过期 → 保持 +2（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        std::cout << "  [T5630.e] 4 回合窗通过\n";
    }

    std::cout << "scenario_5630 PASS (J232 回合消强窗族 1042)" << std::endl;
    return 0;
}
