#include <utils/sbs_rand.h>
// 场景 4800 —— K324 攻技削现血+属性回血族（653，soul_lib/k324_cut_or_heal_dual.cpp）
//（2026-10-21，worktree ../agents/k324；号段 4800-4809 → T4800-T4802 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；max=800000 → 削 1/3 = 266666；对手 NONE）：
//   - T4800 攻击技：对手掉血 = 削 266666 + 红伤 [87,102]（按施加瞬间现血计）。
//   - T4801 属性技：自回血恰 200；对手不动。
//   - T4802 boss 无效：两 rider 均不触发。
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
constexpr int kZhunQue = 20138;
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [1,3,1,2,200]：db[1]=3 削血分母、db[4]=200 属性技回血。
    SoulMark sm(653, "K324 魂印", "test", EffectArgs(std::vector<int>{1, 3, 1, 2, 200}));
    sm.monster_id = 2923;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "佐伊尔", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4800 攻击技：削 1/3 现血 + 红伤 ═══════════════════════════════════════
    {
        sbs_srand(4800);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;   // 800000 → 削 266666
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 266666 + 87 && drop <= 266666 + 102,
              "T4800 653：攻击技 → 削 1/3 现血 266666+红伤（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4800] 攻击技削现血 通过\n";
    }

    // ══ T4801 属性技：自回血恰 200 ═════════════════════════════════════════════
    {
        sbs_srand(4801);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 精准
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 100200,
                 "T4801 653：属性技 → 自回血恰 200（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, foe0,
                 "T4801 653：属性技 → 对手不动（实测变化 -" +
                     std::to_string(foe0 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T4801] 属性技回血 通过\n";
    }

    // ══ T4802 boss 无效：攻击技不削 ════════════════════════════════════════════
    {
        sbs_srand(4802);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102,
              "T4802 653：boss 无效 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4802] boss 无效门 通过\n";
    }

    std::cout << "scenario_4800 PASS (K324 攻技削现血+属性回血族)" << std::endl;
    return 0;
}
