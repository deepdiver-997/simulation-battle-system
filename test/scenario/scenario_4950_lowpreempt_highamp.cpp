#include <utils/sbs_rand.h>
// 场景 4950 —— K339 低体先制/高体增伤双分支族（708，soul_lib/k339_lowpreempt_highamp.cpp）
//（2026-10-21，worktree ../agents/k339；号段 4950-4959 → T4950-T4952 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]，高体 ×1.5 → [130,155]；max=800000；
// 印主速 100 vs 对手 200——先制翻转可观测）：
//   - T4950 低体（300000，×2 < max）：pr 翻印主（先制+1）且只红伤（低体分支无增伤）。
//   - T4951 高体（500000，×2 > max）：pr 归对手且红伤 ×1.5。
//   - T4952 恰 1/2（400000）：双 strict 均不落 → pr 归对手、只红伤。
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
    // DB args 原序 [2,1,50]：db[1]=1 先制量、db[2]=50 增伤%（db[0] 废位）。
    SoulMark sm(708, "K339 魂印", "test", EffectArgs(std::vector<int>{2, 1, 50}));
    sm.monster_id = 3013;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "时空界王", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 设印主血后对拍一刀，返回对手掉血。
int round_with_hp(ScenarioBattle& b, int hp) {
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = hp;
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(4950);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    // ══ T4950 低体分支：先制翻转 + 只红伤 ═════════════════════════════════════
    {
        const int drop = round_with_hp(b, 300000);
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4950 708：低体 → 先制+1 翻转 pr（速 100 压 200）");
        CHECK(drop >= 87 && drop <= 102,
              "T4950 708：低体分支无增伤 → 红伤带 [87,102]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4950] 低体先制分支 通过\n";
    }

    // ══ T4951 高体分支：增伤 ×1.5、无先制 ═════════════════════════════════════
    {
        const int drop = round_with_hp(b, 500000);
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4951 708：高体 → 先制门闭（速 200 对手先）");
        CHECK(drop >= 130 && drop <= 155,
              "T4951 708：高体 → 红伤 ×1.5 ∈ [130,155]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4951] 高体增伤分支 通过\n";
    }

    // ══ T4952 恰 1/2：双 strict 均不落 ═════════════════════════════════════════
    {
        const int drop = round_with_hp(b, 400000);
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4952 708：恰 1/2 → 先制门闭");
        CHECK(drop >= 87 && drop <= 102,
              "T4952 708：恰 1/2 → 无增伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4952] 半线边界 通过\n";
    }

    std::cout << "scenario_4950 PASS (K339 低体先制/高体增伤双分支族)" << std::endl;
    return 0;
}
