#include <utils/sbs_rand.h>
// 场景 4460 —— K290 害怕门威力翻倍+回合回血族（652，soul_lib/k290_fear_power_heal.cpp）
//（2026-10-21，worktree ../agents/k290；号段 4460-4469 → T4460-T4462 三块）
//
// 断言口径（拍打 atk=300 → 红伤带 [87,102]，×2 → [174,204]，两带不相交；对手 NONE）：
//   - T4460 双 rider：对手带害怕 → 掉血 ∈ [174,204]（威力 ×2）且自回血恰 150
//     （hp=1000 起测，同回合两 rider 并行）。
//   - T4461 门闭：对手无异常 → 掉血 ∈ [87,102]，回血仍 +150（回血无条件）。
//   - T4462 门翻：害怕 1 回合窗 → R2 掉血回 [87,102]，回血继续。
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
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);

SoulMark make_mark() {
    // DB args 原序 [6,2,150]：db[0]=6 门码、db[1]=2 倍数、db[2]=150 回血。
    SoulMark sm(652, "K290 魂印", "test", EffectArgs(std::vector<int>{6, 2, 150}));
    sm.monster_id = 2915;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "绝杀·罗格尔", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, 800000, make_mark());
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

int slap_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4460 门开：威力 ×2 + 回血 150 同回合并行 ══════════════════════════════
    {
        sbs_srand(4460);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFear, /*duration_rounds=*/9, /*actor=*/0);
        ctx.getPet(0).hp = 1000;
        const int drop = slap_round(b);
        CHECK(drop >= 174 && drop <= 204,
              "T4460 652：对手害怕 → 掉血 ×2 带 [174,204]（实测 " +
                  std::to_string(drop) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 1150,
                 "T4460 652：同回合回血恰 150（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4460] 门开双 rider 通过\n";
    }

    // ══ T4461 门闭：纯红伤，回血仍发 ═══════════════════════════════════════════
    {
        sbs_srand(4461);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4461 652：对手无害怕 → 纯红伤带 [87,102]（实测 " +
                  std::to_string(drop) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 1150,
                 "T4461 652：回血无条件仍 +150（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4461] 门闭对照 通过\n";
    }

    // ══ T4462 门翻：害怕到期 → 威力回归，回血继续 ══════════════════════════════
    {
        sbs_srand(4462);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFear, /*duration_rounds=*/1, /*actor=*/0);
        ctx.getPet(0).hp = 1000;
        const int drop1 = slap_round(b);   // R1：害怕在 → ×2
        CHECK(drop1 >= 174 && drop1 <= 204, "T4462 652：R1 害怕门开（实测 " +
                                                 std::to_string(drop1) + "）");
        const int drop2 = slap_round(b);   // R2：害怕到期 → 回归
        CHECK(drop2 >= 87 && drop2 <= 102,
              "T4462 652：R2 害怕到期 → 纯红伤带（实测 " + std::to_string(drop2) +
                  "）");
        CHECK_EQ(ctx.getPet(0).hp, 1300,
                 "T4462 652：两回合各回血 150（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4462] 门翻回归 通过\n";
    }

    std::cout << "scenario_4460 PASS (K290 害怕门威力翻倍+回合回血族)" << std::endl;
    return 0;
}
