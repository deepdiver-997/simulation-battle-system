#include <utils/sbs_rand.h>
// 场景 4260 —— K270 致死残血+对手低体门族（365，soul_lib/k270_foe_lowhp_deathgate.cpp）
//（2026-10-21，worktree ../agents/k270；号段 4260-4269 → T4260-T4262 三块）
//
// 断言口径（印主 hp=60/atk=1/速 300 先手；对手 atk=300/速 100 后手，拍打红伤带
// [87,102] 恒 >60 → 对手一刀必致死；85%~100% 浮动不进门判）：
//   - T4260 门开/门闭对照：对手 hp=1000（max 800000，×4 < max = 不足1/4）→ 印主
//     致死后拉起恰 1 点；对手满血 → 门闭死亡（hp=0）。
//   - T4261 边界严格性：对手 hp=200000（恰 = max/4，"不足1/4"不含等号）→ 门闭死亡。
//   - T4262 boss 无效：is_boss_challenge=true + 门开 → 不拉（死亡成立）。
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
constexpr int kMarkHp = 60;        // < 红伤带下限 87 → 对手一刀必致死
constexpr int kFoeMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [4,1]：db[0]=4（1/4 分母）、db[1]=1（残留点数）。
    SoulMark sm(365, "K270 魂印", "test", EffectArgs(std::vector<int>{4, 1}));
    sm.monster_id = 2769;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "真龙·无上", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMarkHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kFoeMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 一回合互拍（印主先手小挠，对手后手致死刀），回对手血量后驱动。
void fatal_round(ScenarioBattle& b, int foe_hp) {
    b.ctx().getPet(1).hp = foe_hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4260.a 门开：对手 hp=1000（不足1/4）→ 致死后拉起恰 1 点 ═══════════════
    {
        sbs_srand(4260);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        fatal_round(b, 1000);
        CHECK_EQ(b.ctx().getPet(0).hp, 1,
                 "T4260.a 365：致死攻击 + 对手不足1/4 → 残留 1 点体力（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4260.a] 门开残留 1 点 通过\n";
    }

    // ══ T4260.b 门闭对照：对手满血 → 门闭死亡 ══════════════════════════════════
    {
        sbs_srand(4261);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        fatal_round(b, kFoeMaxHp);
        CHECK_EQ(b.ctx().getPet(0).hp, 0,
                 "T4260.b 365：对手满血 → 门闭，致死成立（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4260.b] 门闭死亡对照 通过\n";
    }

    // ══ T4261 边界：对手 hp 恰 = max/4 → "不足1/4"不含等号 → 门闭死亡 ═════════
    //（印主先手挠痒刀 1-2 点：初始 200002 补偿蹭伤 → 结算门时血 ∈ [200000,200001]，
    //  ×4 ≥ max 恒成立，边界判定不被蹭伤穿越）
    {
        sbs_srand(4262);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        fatal_round(b, kFoeMaxHp / 4 + 2);
        CHECK_EQ(b.ctx().getPet(0).hp, 0,
                 "T4261 365：对手结算时 hp 恰 ≥ max/4 → 非'不足'，门闭死亡（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4261] 半线严格边界 通过\n";
    }

    // ══ T4262 boss 无效：boss 挑战 + 门开 → 不拉 ═══════════════════════════════
    {
        sbs_srand(4263);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().is_boss_challenge = true;
        fatal_round(b, 1000);
        CHECK_EQ(b.ctx().getPet(0).hp, 0,
                 "T4262 365：boss 无效子句 → 门开也不拉（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4262] boss 无效门 通过\n";
    }

    std::cout << "scenario_4260 PASS (K270 致死残血+对手低体门族)" << std::endl;
    return 0;
}
