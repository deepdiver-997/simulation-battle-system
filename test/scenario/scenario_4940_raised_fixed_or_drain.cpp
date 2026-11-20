#include <utils/sbs_rand.h>
// 场景 4940 —— K338 提升态固伤/未触发吸取族（701，soul_lib/k338_raised_fixed_or_drain.cpp）
//（2026-10-21，worktree ../agents/k338；号段 4940-4949 → T4940-T4942 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；开门直写中性速度槽，K283 探针口径）：
//   - T4940 一态：攻击 → 掉血 = 红伤+50 ∈ [137,152]（提升态 → 不吸取）。
//   - T4941 无提升 idle：吸取恰 200 双向（对手 -200、自身 +200）。
//   - T4942 两态：攻击 → 红伤+100 ∈ [187,202]（每态 50 叠加直证）。
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
    // DB args 原序 [50,200]：db[0]=50 每态固伤、db[1]=200 未触发吸取量。
    SoulMark sm(701, "K338 魂印", "test", EffectArgs(std::vector<int>{50, 200}));
    sm.monster_id = 2999;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "千夜死神", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
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

int mark_attack_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4940 一态：固伤 +50，不吸取 ═══════════════════════════════════════════
    {
        sbs_srand(4940);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][4] = 1;   // 速度槽 +1（1 种提升态）
        const int drop = mark_attack_round(b);
        CHECK(drop >= 137 && drop <= 152,
              "T4940 701：一态 → 红伤+50 ∈ [137,152]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4940] 一态固伤 通过\n";
    }

    // ══ T4941 无提升 idle：吸取恰 200 双向 ═════════════════════════════════════
    {
        sbs_srand(4941);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        idle_round(b);
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 200,
                 "T4941 701：未触发 → 吸取对手 200（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 100200,
                 "T4941 701：自身等量恢复 +200（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4941] 未触发吸取 通过\n";
    }

    // ══ T4942 两态：固伤 +100 ══════════════════════════════════════════════════
    {
        sbs_srand(4942);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][2] = 1;
        ctx.ability_levels[0][4] = 1;   // 两种提升态
        const int drop = mark_attack_round(b);
        CHECK(drop >= 187 && drop <= 202,
              "T4942 701：两态 → 红伤+100 ∈ [187,202]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4942] 两态叠加 通过\n";
    }

    std::cout << "scenario_4940 PASS (K338 提升态固伤/未触发吸取族)" << std::endl;
    return 0;
}
