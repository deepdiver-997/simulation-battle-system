#include <utils/sbs_rand.h>
// 场景 4790 —— K323 攻消回合+全PP压制族（614，soul_lib/k323_attack_break_ppdrain.cpp）
//（2026-10-21，worktree ../agents/k323；号段 4790-4799 → T4790-T4792 三块）
//
// 断言口径（对手 5 技全拍打 PP=35；印主命中必中）：
//   - T4790 命中：每刀对手 PP 总量 -5（全技能各 -1）；两刀 -10。
//   - T4791 未命中（accuracy=0）：PP 不动。
//   - T4792 boss 无效：PP 不动。
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
    // DB args 原序 [1,1,1,1]（全定值语义，tips 数值）。
    SoulMark sm(614, "K323 魂印", "test", EffectArgs(std::vector<int>{1, 1, 1, 1}));
    sm.monster_id = 2862;
    return sm;
}

ElfPet mark_pet(int accuracy) {
    ElfPet p = make_pet(1, "苏希斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, 800000, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = accuracy;
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

int foe_pp_sum(BattleContext& ctx) {
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        sum += ctx.getPet(1).skills[i].pp;
    }
    return sum;
}

void attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4790 命中：每刀 PP 总量 -5 ════════════════════════════════════════════
    {
        sbs_srand(4790);
        ScenarioBattle b(make_party(mark_pet(/*accuracy=*/100)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int sum0 = foe_pp_sum(ctx);
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0 - 5,
                 "T4790 614：命中 → 全技能各 -1（总量 -5，实测 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0 - 10,
                 "T4790 614：第二刀再 -5（实测总量 " + std::to_string(foe_pp_sum(ctx)) +
                     "）");
        std::cout << "  [T4790] 命中全 PP 压制 通过\n";
    }

    // ══ T4791 未命中：PP 不动 ══════════════════════════════════════════════════
    {
        sbs_srand(4791);
        ScenarioBattle b(make_party(mark_pet(/*accuracy=*/0)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int sum0 = foe_pp_sum(ctx);
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0,
                 "T4791 614：未命中 → PP 不动（实测总量 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        std::cout << "  [T4791] 未命中锚 通过\n";
    }

    // ══ T4792 boss 无效：PP 不动 ═══════════════════════════════════════════════
    {
        sbs_srand(4792);
        ScenarioBattle b(make_party(mark_pet(/*accuracy=*/100)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        const int sum0 = foe_pp_sum(ctx);
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0,
                 "T4792 614：boss 无效 → PP 不动（实测总量 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        std::cout << "  [T4792] boss 无效门 通过\n";
    }

    std::cout << "scenario_4790 PASS (K323 攻消回合+全PP压制族)" << std::endl;
    return 0;
}
