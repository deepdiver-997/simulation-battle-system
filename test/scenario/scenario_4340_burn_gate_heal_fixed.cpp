#include <utils/sbs_rand.h>
// 场景 4340 —— K278 烧伤门回血/固伤族（618，soul_lib/k285_burngate_heal_fixed.cpp（K278 已收敛））
//（2026-10-21，worktree ../agents/k278；号段 4340-4349 → T4340-T4341 两块）
//
// 断言口径（对手全程 NONE：无蹭伤扰动；烧伤用场景侧 apply_anomaly 直造，9 回合窗）：
//   - T4340 门开：对手带烧伤 → 首个 ROUND_START 自回血恰 200（低体起测防 max 钳）。
//   - T4341 门闭：对手无异常 → 对手掉血恰 200（粉伤无浮动）且自身 hp 不变
//     （互斥分支交叉门）。
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark() {
    // DB args 原序 [2,200,200]：db[1]=200 门开回血、db[2]=200 门闭固伤。
    SoulMark sm(618, "K278 魂印", "test", EffectArgs(std::vector<int>{2, 200, 200}));
    sm.monster_id = 2869;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "弗雷姆", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, 800000, make_mark());
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

    // ══ T4340 门开：对手带烧伤 → 回血恰 200 ════════════════════════════════════
    {
        sbs_srand(4340);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
        ctx.getPet(0).hp = 1000;   // 低体起测（远离 max 钳）
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).hp, 1200,
                 "T4340 618：对手烧伤 → 回血恰 200（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4340] 烧伤门开回血 通过\n";
    }

    // ══ T4341 门闭：对手无异常 → 固伤恰 200，自身无回血 ════════════════════════
    {
        sbs_srand(4341);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        idle_round(b);
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 200,
                 "T4341 618：对手无烧伤 → 固伤恰 200（实测 -" +
                     std::to_string(foe0 - ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, hp0,
                 "T4341 618：门闭不回血（实测变化 " +
                     std::to_string(ctx.getPet(0).hp - hp0) + "）");
        std::cout << "  [T4341] 门闭固伤 通过\n";
    }

    std::cout << "scenario_4340 PASS (K278 烧伤门回血/固伤族)" << std::endl;
    return 0;
}
