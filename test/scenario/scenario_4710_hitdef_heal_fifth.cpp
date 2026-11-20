#include <utils/sbs_rand.h>
// 场景 4710 —— K315 受击双防+回合末回血族（672，soul_lib/k315_hitdef_heal_fifth.cpp）
//（2026-10-21，worktree ../agents/k315；号段 4710-4719 → T4710-T4711 两块）
//
// 断言口径（max=800000 → 回血 160000；印主速 300 先手 NONE，对手拍打）：
//   - T4710 受击两连：双防各 +2 累计；每回合末回血 160000（受击挠伤 1-2 → 带宽断言）。
//   - T4711 未受击：纯回血恰 160000。
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
    // DB args 原序 [1,1,1,3,1,5]：db[5]=5 回血分母（余废位）。
    SoulMark sm(672, "K315 魂印", "test", EffectArgs(std::vector<int>{1, 1, 1, 3, 1, 5}));
    sm.monster_id = 2947;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "利刃之主鲁斯王", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_pair(ScenarioBattle& b, bool foe_attacks) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, foe_attacks ? BattleFsm::ActionType::SELECT_SKILL
                                      : BattleFsm::ActionType::NONE,
                       foe_attacks ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4710 受击两连：双防累计 +2，回血照发 ══════════════════════════════════
    {
        sbs_srand(4710);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        round_pair(b, /*foe_attacks=*/true);   // R1：受击
        CHECK_EQ(ctx.ability_levels[0][2], 1,
                 "T4710 672：R1 受击 → 防御 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][2]) + "）");
        round_pair(b, /*foe_attacks=*/true);   // R2：再受击
        CHECK_EQ(ctx.ability_levels[0][2], 2,
                 "T4710 672：R2 再受击 → 防御累计 +2（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][2]) + "）");
        CHECK_EQ(ctx.ability_levels[0][3], 2,
                 "T4710 672：特防累计 +2（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][3]) + "）");
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 419990 && hp <= 420005,
              "T4710 672：两回合各回血 160000（净 +320000）扣两次挠伤（实测 hp=" +
                  std::to_string(hp) + "）");
        std::cout << "  [T4710] 受击双防累计/回血 通过\n";
    }

    // ══ T4711 未受击：纯回血恰 160000 ══════════════════════════════════════════
    {
        sbs_srand(4711);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        round_pair(b, /*foe_attacks=*/false);
        CHECK_EQ(ctx.getPet(0).hp, 260000,
                 "T4711 672：未受击 → 回血 1/5 = 160000（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4711] 纯回血 通过\n";
    }

    std::cout << "scenario_4710 PASS (K315 受击双防+回合末回血族)" << std::endl;
    return 0;
}
