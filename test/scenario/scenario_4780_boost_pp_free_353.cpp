// 场景 4780 —— K322 强化态免耗 PP 族（353，soul_lib/k322_boost_pp_free.cpp）
//（2026-10-21，worktree ../agents/k322；号段 4780-4789 → T4780-T4782 三块）
//
// 断言口径（353「自身处于能力提升时 使用技能不消耗PP」）：
//   - T4780 **免耗生效**：印主处于能力提升状态时用技能 → 技能 PP **不减少**
//     （对照：无提升时用同样技能 → PP -1）。
//   - T4781 **门闭对照**：印主**不**处于能力提升状态 → 用技能 → PP -1（正常消耗）。
//   - T4782 **倍率槽断言**：免耗时 ws.skill_pp_cost_multiplier[0] == 0；
//     门闭时为 1（回合初 reset 回 1 的默认值——本单**不留残留**）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,3}（不补 {0,0} 前缀）——实现侧
//   **不消费任何下标**（args 是废位，按 tips 字面定值）。
// ⚠️ **PP 消耗点在 AFTER_ACTION**（battleFsm.cpp:457 扣 pp）→ 断言读**整回合
//   结束后**的 pp 值（场景 drive() 跑完整回合后读）。
// ⚠️ **能力提升状态的造法**：直接写 ctx.ability_levels[0][4] = 1（速度槽，+1）——
//   攻击槽会放大伤害混入变量（K289 306 教训），速度槽不参与伤害公式。
// ⚠️ 拍打 kJi 的 PP 消耗 = 1（标准单次技能）→ 正常回合 pp -1、免耗回合 pp 不变。
#include <cstdlib>
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

constexpr int kJi = 10001;    // 拍打：单次消耗 1 点 PP
constexpr int kMaxHp = 99999;
constexpr int kBoostSlot = 4; // 槽 4 = 速度（造"能力提升"用，不影响伤害公式）

SoulMark make_mark() {
    // DB args 原样 [1,3]（废位）；实现不消费任何下标。
    SoulMark sm(353, "K322 魂印", "test", EffectArgs(std::vector<int>{1, 3}));
    sm.monster_id = 2733;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "初夏", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/10, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4780 免耗生效：处于能力提升状态 → PP 不变 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kBoostSlot] = 1;   // 造"能力提升状态"
        REQUIRE(ctx.ability_levels[0][kBoostSlot] > 0);
        const int pp_before = ctx.getPet(0).skills[0].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_before,
                 "T4780 353：处于能力提升状态 → 使用技能不消耗 PP（PP "
                     + std::to_string(pp_before) + " → "
                     + std::to_string(ctx.getPet(0).skills[0].pp) + "）");
        std::cout << "  [T4780] 免耗生效 通过\n";
    }

    // ══ T4781 门闭对照：无提升状态 → PP 正常 -1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp_before = ctx.getPet(0).skills[0].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_before - 1,
                 "T4781 353：不处于能力提升状态 → 正常消耗 1 点 PP（"
                     + std::to_string(pp_before) + " → "
                     + std::to_string(ctx.getPet(0).skills[0].pp) + "）");
        std::cout << "  [T4781] 门闭对照 通过\n";
    }

    // ══ T4782 倍率槽断言：免耗时 0、门闭时 1（回合初默认值，无残留）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kBoostSlot] = 1;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_pp_cost_multiplier[0], 0,
                 "T4782 353：免耗时 PP 消耗倍率槽 = 0（内核读到 ≤0 → 不扣 PP），实测 "
                     + std::to_string(ctx.ws.skill_pp_cost_multiplier[0]));
        // 下一回合：门不成立（提升没了）→ 倍率应回 1（回合初 reset），无残留。
        ctx.ability_levels[0][kBoostSlot] = 0;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.ws.skill_pp_cost_multiplier[0] != 0,
              "T4782 353：门不成立的后续回合 → 倍率槽回 1（回合初 reset，**无残留**），实测 "
                  + std::to_string(ctx.ws.skill_pp_cost_multiplier[0]));
        std::cout << "  [T4782] 倍率槽无残留 通过\n";
    }

    std::cout << "scenario_4780 PASS (K322 强化态免耗 PP 族)" << std::endl;
    return 0;
}
