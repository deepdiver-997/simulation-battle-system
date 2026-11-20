#include <utils/sbs_rand.h>
// 场景 4990 —— K343 入场封属性技族（289，soul_lib/k343_entry_seal_attribute.cpp）
//（2026-10-21，worktree ../agents/k343；号段 4990-4999 → T4990 一块三断言）
//
// 断言口径（对手用精准（属性技，命中+1）探封印；行为断言 = 属性技效果是否落地）：
//   R1/R2（封印窗）：对手精准整回合无效 → 对手命中等级不动。
//   R3（窗过期）：对手精准正常 +1。
//   无印对照：R1 精准正常 +1。
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
constexpr int kZhunQue = 20138;   // 精准：属性技（自身命中+1，封印探针）

SoulMark make_mark() {
    // DB args 原序 [2,1]：db[0]=2 封印回合数（db[1] 废位）。
    SoulMark sm(289, "K343 魂印", "test", EffectArgs(std::vector<int>{2, 1}));
    sm.monster_id = 2545;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "苍魂·青龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, 800000, make_mark())
        : make_pet(1, "裸青龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_precision_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4990 封印窗 R1/R2 无效、R3 过期恢复 ═══════════════════════════════════
    {
        sbs_srand(4990);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        foe_precision_round(b);   // R1：窗内 → 精准无效
        CHECK_EQ(ctx.ability_levels[1][5], 0,
                 "T4990 289：R1 封印窗 → 对手精准无效（命中 lv=" +
                     std::to_string(ctx.ability_levels[1][5]) + "）");
        foe_precision_round(b);   // R2：窗内
        CHECK_EQ(ctx.ability_levels[1][5], 0,
                 "T4990 289：R2 仍封印（命中 lv=" +
                     std::to_string(ctx.ability_levels[1][5]) + "）");
        foe_precision_round(b);   // R3：窗过期
        CHECK_EQ(ctx.ability_levels[1][5], 1,
                 "T4990 289：R3 窗过期 → 精准正常 +1（命中 lv=" +
                     std::to_string(ctx.ability_levels[1][5]) + "）");
        std::cout << "  [T4990] 封印窗/过期恢复 通过\n";
    }

    // ══ T4991 无印对照：R1 精准正常 ════════════════════════════════════════════
    {
        sbs_srand(4991);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        foe_precision_round(b);
        CHECK_EQ(b.ctx().ability_levels[1][5], 1,
                 "T4991 289：无印对照 → 对手精准正常 +1（命中 lv=" +
                     std::to_string(b.ctx().ability_levels[1][5]) + "）");
        std::cout << "  [T4991] 无印对照 通过\n";
    }

    std::cout << "scenario_4990 PASS (K343 入场封属性技族)" << std::endl;
    return 0;
}
