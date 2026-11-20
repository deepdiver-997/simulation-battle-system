// 场景 1300 —— J45 低体先手免伤族（972，moves_lib/j_lowhp_first_immune.cpp）
//（2026-10-02，worktree ../agents/j45；号段 1300-1309 → T1300 一块）
//
// 断言口径：
//   - 19723 枯萎风夜颂 [1]（counts=1）：低体施放 → 免 1 次对手攻击（hp 不动）→
//     第 2 刀落地（票已耗）；高体对照 → 不免首刀即掉血。
//   foe atk=100 刀 ~30，hp 5000/3000 分层防击杀。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对手填充刀）
constexpr int kKuWei   = 19723;   // 枯萎风夜颂 [972]|[1] 特130（972 counts=1）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 3000) {
    ElfPet p = make_pet(1, "低体者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1300 972 低体先手免伤（19723 [1]；我 hp 3000 < 对手 99999 恒低体）══
    {
        {   // (a) 低体施放 → 免 1 刀，第 2 刀落地
            ScenarioBattle b(make_party(self_pet({kKuWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 枯萎风夜颂
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手刀 1（被免）
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 3000, "T1300.a 972：低体 → 首刀被免（hp 不动）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 刀 2（票已耗）
            b.drive();
            CHECK(ctx.getPet(0).hp < 3000, "T1300.a 972：第 2 刀落地（counts=1 耗尽）");
        }
        {   // (b) 高体对照（我 hp 5000 > 对手 99999？——反向：把对手压到低血）→ 不免
            ScenarioBattle b(make_party(self_pet({kKuWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/100, /*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我 hp 5000 > 3000
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手刀（不免）
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1300.b 972 门：高体不免（首刀即掉血）");
        }
        std::cout << "  [T1300] 972 低体先手免伤+门 通过\n";
    }

    std::cout << "scenario_1300 PASS (J45 低体先手免伤族)" << std::endl;
    return 0;
}
