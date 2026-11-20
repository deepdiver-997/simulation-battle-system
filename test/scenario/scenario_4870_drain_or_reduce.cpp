#include <utils/sbs_rand.h>
// 场景 4870 —— K331 双分支吸取/减伤族（683，soul_lib/k331_drain_or_reduce.cpp）
//（2026-10-21，worktree ../agents/k331；号段 4870-4879 → T4870-T4872 三块）
//
// 断言口径（max=800000 → 吸取 200000；对手 NONE/拍打 atk=300 红伤 [87,102]）：
//   - T4870 低体分支：印主 400000 < 对手 800000 → drain 200000（对手 -200000、
//     自身 +200000）。
//   - T4871 高体分支：印主满血 > 对手（压到 400000）→ 不吸取且减伤 30% 生效
//     （对手刀 → 实扣 = 红伤×0.7 ∈ [60,72]）。
//   - T4872 boss 无效：低体也不吸取。
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
    // DB args 原序 [4,1,30]：db[0]=4 吸取分母、db[2]=30 减伤%（db[1] 废位）。
    SoulMark sm(683, "K331 魂印", "test", EffectArgs(std::vector<int>{4, 1, 30}));
    sm.monster_id = 2966;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "生灵之主·蒙娜丽莎", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

void round_ops(ScenarioBattle& b, bool foe_attacks) {
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

    // ══ T4870 低体分支：drain 1/4 ══════════════════════════════════════════════
    {
        sbs_srand(4870);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400000;
        round_ops(b, /*foe_attacks=*/false);
        CHECK_EQ(ctx.getPet(1).hp, 600000,
                 "T4870 683：低体 → 吸取对手 1/4 = 200000（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 600000,
                 "T4870 683：自身等量恢复（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4870] 低体吸取分支 通过\n";
    }

    // ══ T4871 高体分支：减伤 30% ═══════════════════════════════════════════════
    {
        sbs_srand(4871);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 400000;   // 对手压低 → 印主高体分支
        const int hp0 = ctx.getPet(0).hp;
        round_ops(b, /*foe_attacks=*/true);
        const int taken = hp0 - ctx.getPet(0).hp;
        CHECK(taken >= 60 && taken <= 72,
              "T4871 683：高体 → 减伤 30%（红伤×0.7 ∈ [60,72]，实测 -" +
                  std::to_string(taken) + "）");
        CHECK_EQ(ctx.getPet(1).hp, 400000,
                 "T4871 683：高体 → 不吸取（对手血不变）");
        std::cout << "  [T4871] 高体减伤分支 通过\n";
    }

    // ══ T4872 boss 无效：低体也不吸取 ══════════════════════════════════════════
    {
        sbs_srand(4872);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400000;
        round_ops(b, /*foe_attacks=*/false);
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp,
                 "T4872 683：boss 无效 → 不吸取（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T4872] boss 无效门 通过\n";
    }

    std::cout << "scenario_4870 PASS (K331 双分支吸取/减伤族)" << std::endl;
    return 0;
}
