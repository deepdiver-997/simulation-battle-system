// 场景 3650 —— J184 PP 自由窗族（2232，moves_lib/j_pp_free_window.cpp）
//（2026-10-07，worktree ../agents/j184；号段 3650-3659 → T3650-T3651 两块）
//
// 断言口径（真载体 28867 {0}=2 / 28957 {0}=1，链上 1687 已实现但为受击弹控窗、
// 不涉 PP；未实现 2230/2229/2231 哑）：
//   - 当回合（使用回合）不生效（NextRounds 口径）：ignore_pp=false、PP=0 被拒；
//   - 窓内（下 N 回合）：每回合 ROUND_START tick 重写 ignore_pp=true，
//     PP=0 技 SELECTABLE；
//   - 过期：窗界外 tick 不再跑、ignore_pp 回 false、PP=0 被拒。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打 filler
constexpr int kCarrier2 = 28867;   // 棬壑孽海 {0}=2（cat4 属性技）
constexpr int kCarrier1 = 28957;   // 不朽荣光 {0}=1

ElfPet carrier_pet(int carrier_id) {
    ElfPet p = make_pet(1, "印主", {carrier_id, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_act(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3650 载体 28867 {0}=2：当回合不生效 → R2/R3 生效 → R4 过期 ══
    {
        ScenarioBattle b(make_party(carrier_pet(kCarrier2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：使用载体（SKILL_EFFECT 落窗，NextRounds 不含当回合）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.ignore_pp[0], "T3650 使用当回合不受保护（NextRounds 口径）");
        ctx.getPet(0).skills[1].pp = 0;
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::PP_EMPTY,
              "T3650 当回合 PP=0 仍被拒");
        // R2：窗内第一回合
        both_act(b);
        CHECK(ctx.ignore_pp[0], "T3650 R2 tick 重写 ignore_pp（每回合重置后）");
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::SELECTABLE,
              "T3650 R2 PP=0 可选（窗内）");
        // R3：窗内第二回合（{0}=2 → [R1+1, R1+2]）
        both_act(b);
        CHECK(ctx.ignore_pp[0], "T3650 R3 仍在窗内");
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::SELECTABLE,
              "T3650 R3 PP=0 仍可选");
        // R4：过期（tick 不再跑，ignore_pp 回 false）
        both_act(b);
        CHECK(!ctx.ignore_pp[0], "T3650 R4 窓过期 ignore_pp 回 false");
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::PP_EMPTY,
              "T3650 R4 PP=0 重新被拒");
        std::cout << "  [T3650] 2232 {0}=2 窓界 通过\n";
    }

    // ══ T3651 载体 28957 {0}=1：仅 R2 生效 ══
    {
        ScenarioBattle b(make_party(carrier_pet(kCarrier1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.ignore_pp[0], "T3651 使用当回合不生效");
        both_act(b);
        CHECK(ctx.ignore_pp[0], "T3651 R2 窓内生效");
        ctx.getPet(0).skills[1].pp = 0;
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::SELECTABLE,
              "T3651 R2 PP=0 可选");
        both_act(b);
        CHECK(!ctx.ignore_pp[0], "T3651 R3 {0}=1 窓已过期");
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::PP_EMPTY,
              "T3651 R3 PP=0 重新被拒");
        std::cout << "  [T3651] 2232 {0}=1 窓界 通过\n";
    }

    std::cout << "scenario_3650 PASS (J184 PP 自由窗族)" << std::endl;
    return 0;
}
