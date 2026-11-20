#include <utils/sbs_rand.h>
// 场景 4320 —— K276 攻技回血/属技固伤双 rider 族（606，soul_lib/k276_dual_rider_heal_fixed.cpp）
//（2026-10-21，worktree ../agents/k276；号段 4320-4329 → T4320-T4321 两块）
//
// 断言口径（对手全程 NONE：无蹭伤、无还手，rider 逐刀精确计量）：
//   - T4320 攻击技 rider：拍打（atk=300）→ 自 hp +120 恰值（低体起测防 max 钳），
//     且对手掉血 ∈ [87,102]（纯红伤带——攻击技不触发固伤 rider 的交叉门）。
//   - T4321 属性技 rider：精准 → 对手掉血恰 120（粉伤无浮动），自身 hp 变化恰 0
//     （属性技不触发回血 rider 的交叉门）。
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
constexpr int kZhunQue = 20138;   // 精准：属性技（无红伤）

SoulMark make_mark() {
    // DB args 原序 [1,120,2,120]：db[1]=120 攻击技回血、db[3]=120 属性技固伤。
    SoulMark sm(606, "K276 魂印", "test", EffectArgs(std::vector<int>{1, 120, 2, 120}));
    sm.monster_id = 2841;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "尤西比奥", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4320 攻击技 rider：自回血恰 120；对手纯红伤（无 120 固伤混入）═════════
    {
        sbs_srand(4320);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 低体起测（远离 max 钳）
        const int hp0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp0 + 120,
                 "T4320 606：攻击技 → 自回血恰 120（实测 +" +
                     std::to_string(ctx.getPet(0).hp - hp0) + "）");
        const int foe_drop = foe0 - ctx.getPet(1).hp;
        CHECK(foe_drop >= 87 && foe_drop <= 102,
              "T4320 606：攻击技对手掉血 = 纯红伤带 [87,102]（实测 " +
                  std::to_string(foe_drop) + "，若混入固伤 rider 会 ≥287）");
        std::cout << "  [T4320] 攻击技回血 rider 通过\n";
    }

    // ══ T4321 属性技 rider：对手掉血恰 120；自身无回血 ═════════════════════════
    {
        sbs_srand(4321);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 精准
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 120,
                 "T4321 606：属性技 → 对手固伤恰 120（实测 -" +
                     std::to_string(foe0 - ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, hp0,
                 "T4321 606：属性技不触发回血 rider（实测变化 " +
                     std::to_string(ctx.getPet(0).hp - hp0) + "）");
        std::cout << "  [T4321] 属性技固伤 rider 通过\n";
    }

    std::cout << "scenario_4320 PASS (K276 攻技回血/属技固伤双 rider 族)" << std::endl;
    return 0;
}
