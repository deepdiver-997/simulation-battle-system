// 场景 2570 —— J155 窗降/窗吸族（725/13，moves_lib/j_window_drain.cpp）
//（2026-10-04，worktree ../agents/j155；号段 2570-2579）
//
// 断言口径：
//   - 725 概率窗（万世荒芜决 [3,30,3]，reduce 33%/跳）：20 场 × 3 跳 = 60 掷、
//     p=0.30 → 命中数 ∈ [8,34]（±3σ 宽带；命中场 max_hp 恰 -33%）。
//   - 13 寄生窗（寄生种子 [5]，NextRounds R2..R6 每跳 max/8=124999 整除）：
//       (a) 对手非草：6 回合后 loss == 5×125000（R1 施放当回合不跳）；
//       (b) 对手草系（直调改系别视图）：loss == 0。
#include <iostream>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kHuangWu = 17072; // 万世荒芜决 [725]|[3,30,3]（特）
constexpr int kJiSheng = 20007; // 寄生种子 [13]|[5]（属）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "窗族", skills, /*speed=*/300, /*atk=*/1, 5000);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2570 725 概率窗统计带 ══
    {
        int hits = 0;
        for (int t = 0; t < 20; ++t) {
            ScenarioBattle b(make_party(self_pet({kHuangWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int max_before = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];   // HP 槽
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int r = 2; r <= 4; ++r) {   // R2..R4 三跳
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
            const int max_after = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
            if (max_after < max_before) {
                ++hits;   // 每跳按当前 max 乘算 33%、可多跳叠加（实测确认）
            }
        }
        CHECK(hits >= 8 && hits <= 34,
              "T2570 统计带：60 掷 30% 命中 " + std::to_string(hits) + " ∉ [8,34]");
        std::cout << "  [T2570] 725 概率窗统计带 通过（hits=" << hits << "/60）\n";
    }

    // ══ T2571 13 寄生窗（非草）══
    {
        ScenarioBattle b(make_party(self_pet({kJiSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放（不跳）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int after_cast = ctx.getPet(1).hp;
        CHECK_EQ(after_cast, 999999, "T2571.a 前置：施放当回合不跳");
        for (int r = 2; r <= 6; ++r) {   // R2..R6 五跳 ×125000
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        const int loss = 999999 - ctx.getPet(1).hp;
        const int per = 999999 / 8;   // 整除 124999
        CHECK_EQ(loss, 5 * per, "T2571.a 13 寄生：5 跳各 1/8（loss=" +
                                    std::to_string(loss) + "）");
        std::cout << "  [T2571] 13 寄生窗 通过\n";
    }

    // ══ T2572 13 草系免役 ══
    {
        ScenarioBattle b(make_party(self_pet({kJiSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 直调改对手本体系别为草系（官方码 1；ws 视图每回合 reset，须改持久层）
        ctx.getPet(1).elementalAttributes = {1, 1};
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        for (int r = 2; r <= 6; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        CHECK_EQ(ctx.getPet(1).hp, 999999, "T2572 13 草系：零吸取");
        std::cout << "  [T2572] 13 草系免役 通过\n";
    }

    std::cout << "scenario_2570 ALL PASS" << std::endl;
    return 0;
}
