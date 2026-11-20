#include <utils/sbs_rand.h>
// 场景 4550 —— K299 免疫能力下降族（364，soul_lib/k299_stat_drop_immune.cpp）
//（2026-10-21，worktree ../agents/k299；号段 4550-4559 → T4550-T4552 三块）
//
// 断言口径（行为断言：场景直调 stat_drop/stat_change 原语）：
//   - T4550 印主：stat_drop(攻,-2) 整次失败（等级 0）；提升不受影响（+2 生效）。
//   - T4551 无印对照：stat_drop 正常 -2。
//   - T4552 boss 无效：start 前置旗 → stat_drop 生效。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;

SoulMark make_mark() {
    SoulMark sm(364, "K299 魂印", "test", EffectArgs(std::vector<int>{}));
    sm.monster_id = 2766;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "天狐·勾玉", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, 800000, make_mark())
        : make_pet(1, "裸天狐", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
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

    // ══ T4550 印主：免疫下降、提升照常 ═════════════════════════════════════════
    {
        sbs_srand(4550);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_drop(&ctx, 0, /*stat=*/0, /*amount=*/2, /*actor=*/1);
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T4550 364：免疫能力下降 → stat_drop(2) 等级不动（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        (void)stat_change(&ctx, 0, /*stat=*/0, /*delta=*/2);
        CHECK_EQ(ctx.ability_levels[0][0], 2,
                 "T4550 364：提升不受免疫影响（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        std::cout << "  [T4550] 免疫下降/提升照常 通过\n";
    }

    // ══ T4551 无印对照：下降生效 ═══════════════════════════════════════════════
    {
        sbs_srand(4551);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        (void)stat_drop(&b.ctx(), 0, /*stat=*/0, /*amount=*/2, /*actor=*/1);
        CHECK_EQ(b.ctx().ability_levels[0][0], -2,
                 "T4551 364：无印对照 → stat_drop 正常 -2（实测 lv=" +
                     std::to_string(b.ctx().ability_levels[0][0]) + "）");
        std::cout << "  [T4551] 无印对照 通过\n";
    }

    // ══ T4552 boss 无效：start 前置旗 → 免疫不挂，下降生效 ═════════════════════
    {
        sbs_srand(4552);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        (void)stat_drop(&b.ctx(), 0, /*stat=*/0, /*amount=*/2, /*actor=*/1);
        CHECK_EQ(b.ctx().ability_levels[0][0], -2,
                 "T4552 364：boss 无效 → 免疫不挂，下降生效（实测 lv=" +
                     std::to_string(b.ctx().ability_levels[0][0]) + "）");
        std::cout << "  [T4552] boss 无效门 通过\n";
    }

    std::cout << "scenario_4550 PASS (K299 免疫能力下降族)" << std::endl;
    return 0;
}
