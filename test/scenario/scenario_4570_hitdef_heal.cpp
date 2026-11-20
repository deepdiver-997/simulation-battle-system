#include <utils/sbs_rand.h>
// 场景 4570 —— K301 挨打双防+挨打回血族（1052，soul_lib/k301_hitdef_heal.cpp）
//（2026-10-21，worktree ../agents/k301；号段 4570-4579 → T4570-T4572 三块）
//
// 断言口径（max=800000 → 未受击回血 200000；印主速 300 先手）：
//   - T4570 受击：对手拍打 → 双防各 +1 且当回合不回血（hp 只掉挠伤）。
//   - T4571 未受击：对手 NONE → 回合末回血恰 200000。
//   - T4572 旗重计：R1 受击（不回血）→ R2 未受击（回血 200000，旗已清）。
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
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [1,4]：db[0]=1 双防提升量、db[1]=4 回血分母。
    SoulMark sm(1052, "K301 魂印", "test", EffectArgs(std::vector<int>{1, 4}));
    sm.monster_id = 3453;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "布姆", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_pair(ScenarioBattle& b, bool foe_attacks) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, foe_attacks ? BattleFsm::ActionType::SELECT_SKILL : BattleFsm::ActionType::NONE,
                       foe_attacks ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4570 受击：双防 +1 且不回血 ═══════════════════════════════════════════
    {
        sbs_srand(4570);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        round_pair(b, /*foe_attacks=*/true);
        CHECK_EQ(ctx.ability_levels[0][2], 1,
                 "T4570 1052：受击 → 防御 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][2]) + "）");
        CHECK_EQ(ctx.ability_levels[0][3], 1,
                 "T4570 1052：受击 → 特防 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][3]) + "）");
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 99900 && hp < 100000,
              "T4570 1052：受击当回合不回血（实测 hp=" + std::to_string(hp) + "）");
        std::cout << "  [T4570] 受击双防/不回血 通过\n";
    }

    // ══ T4571 未受击：回合末回血恰 200000 ══════════════════════════════════════
    {
        sbs_srand(4571);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        round_pair(b, /*foe_attacks=*/false);
        CHECK_EQ(ctx.getPet(0).hp, 300000,
                 "T4571 1052：当回合未受击 → 回血 1/4 = 200000（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4571] 未受击回血 通过\n";
    }

    // ══ T4572 旗重计：R1 受击不回血 → R2 未受击回血 ════════════════════════════
    {
        sbs_srand(4572);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        round_pair(b, /*foe_attacks=*/true);   // R1：受击
        const int after_r1 = ctx.getPet(0).hp;
        round_pair(b, /*foe_attacks=*/false);  // R2：未受击 → 回血（旗已清）
        CHECK_EQ(ctx.getPet(0).hp, after_r1 + 200000,
                 "T4572 1052：R2 未受击 → 回血 200000（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "，R1 末 " +
                     std::to_string(after_r1) + "）");
        std::cout << "  [T4572] 旗重计回血 通过\n";
    }

    std::cout << "scenario_4570 PASS (K301 挨打双防+挨打回血族)" << std::endl;
    return 0;
}
