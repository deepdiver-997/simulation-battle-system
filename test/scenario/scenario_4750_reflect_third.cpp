#include <utils/sbs_rand.h>
// 场景 4750 —— K319 反弹族（602，soul_lib/k319_reflect_third.cpp）
//（2026-10-21，worktree ../agents/k319；号段 4750-4759 → T4750-T4751 两块）
//
// 断言口径（对手拍打 atk=300 → 印主受红伤 [87,102] → 反弹 1/3 = [29,34]；对手 NONE）：
//   - T4750 反弹：对手掉血 ∈ [29,34]（FIXED 无浮动）。
//   - T4751 boss 无效：不弹（对手零掉血）。
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
    // DB args 原序 [3,1]：db[0]=3 反弹分母（db[1] 废位）。
    SoulMark sm(602, "K319 魂印", "test", EffectArgs(std::vector<int>{3, 1}));
    sm.monster_id = 2840;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "天影无名", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_attack_round(ScenarioBattle& b) {
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

    // ══ T4750 反弹 1/3 ═════════════════════════════════════════════════════════
    {
        sbs_srand(4750);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        foe_attack_round(b);
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 29 && drop <= 34,
              "T4750 602：受红伤 [87,102] → 反弹 1/3 ∈ [29,34]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4750] 反弹 1/3 通过\n";
    }

    // ══ T4751 boss 无效：不弹 ══════════════════════════════════════════════════
    {
        sbs_srand(4751);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        foe_attack_round(b);
        CHECK_EQ(ctx.getPet(1).hp, foe0,
                 "T4751 602：boss 无效 → 不反弹（实测变化 -" +
                     std::to_string(foe0 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T4751] boss 无效门 通过\n";
    }

    std::cout << "scenario_4750 PASS (K319 反弹族)" << std::endl;
    return 0;
}
