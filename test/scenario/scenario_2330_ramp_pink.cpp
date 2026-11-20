// 场景 2330 —— J138 累积/条件附粉族（1366/903/1132，moves_lib/j_ramp_pink.cpp）
//（2026-10-04，worktree ../agents/j138；号段 2330-2339）
//
// 断言口径（粉=final×pct% 精确可断；红=resolvedDamage.final）：
//   - 1366 累积（战意重击 34267 [540,1366]|[?,20,10,40]；540 后手必暴门——我先手良性）：
//       R1 首用 ×20%；R3 重施 ×30%；R5 ×40%；R7 钳 40%。
//   - 903 高体附粉（笔走龙蛇 30005 [30]）：(a) 我 5000 > 对手 4500 → loss==red+1500；
//     (b) 对手 8000 对照 → loss==red。
//   - 1132 随机带回复（福光普照 30485 [200,400,300,500]）：(a) 有提升 → 回复 ∈[200,400]；
//     (b) 无提升 → ∈[300,500]。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kGaoSu  = 20017;    // 高速移动（造提升）
constexpr int kZhanYi = 34267;    // 战意重击 [540,1366]|[?,20,10,40]（物150）
constexpr int kBiZou  = 30005;    // 笔走龙蛇 [903]|[30]（物130）
constexpr int kFuGuang = 30485;   // 福光普照 [1132]|[200,400,300,500]（特140）
constexpr int kJiGuangDao = 19011; // 激光刀（foe 削血刀）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 5000) {
    ElfPet p = make_pet(1, "附粉族", skills, /*speed=*/300, /*atk=*/1000, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int hp = 999999, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, hp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2330 1366 累积粉 ══
    {
        ScenarioBattle b(make_party(self_pet({kZhanYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = 999999 - ctx.getPet(1).hp;
        for (int round = 1; round <= 7; round += 2) {
            const int expect_pct = std::min(20 + (round - 1) / 2 * 10, 40);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int delta = loss - prev;   // 本轮掉血（loss 为累计值）
            prev = loss;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(delta, red + red * expect_pct / 100,
                     "T2330 R" + std::to_string(round) + "：粉=" + std::to_string(expect_pct) +
                         "%（delta=" + std::to_string(delta) + " red=" + std::to_string(red) + "）");
            if (round + 1 <= 7) {   // 隔一轮 NONE
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
        }
        std::cout << "  [T2330] 1366 累积粉 20→30→40→40(钳) 通过\n";
    }

    // ══ T2331 903 高体附粉 ══
    {
        {   // (a) 我 5000 > 对手 4500 → +1500
            ScenarioBattle b(make_party(self_pet({kBiZou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*hp=*/4500)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 4500 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red + 1500,
                     "T2331.a 903：loss==red+1500（loss=" + std::to_string(loss) + " red=" +
                         std::to_string(red) + "）");
        }
        {   // (b) 对手 8000 对照 → 无粉
            ScenarioBattle b(make_party(self_pet({kBiZou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*hp=*/8000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 8000 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red, "T2331.b 903 对照：loss==red");
        }
        std::cout << "  [T2331] 903 高体附粉+对照 通过\n";
    }

    // ══ T2332 1132 随机带回复 ══
    {
        {   // (a) 有提升 → band1 [200,400]
            ScenarioBattle b(make_party(self_pet({kGaoSu, kFuGuang, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kJiGuangDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                999999, /*atk=*/1100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高速移动
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 削血
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 福光普照
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int healed = ctx.getPet(0).hp - before;
            CHECK(healed >= 200 && healed <= 400,
                  "T2332.a 1132 有提升：回复∈[200,400]（实测 " + std::to_string(healed) + "）");
        }
        {   // (b) 无提升 → band2 [300,500]
            ScenarioBattle b(make_party(self_pet({kFuGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kJiGuangDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                999999, /*atk=*/1100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int healed = ctx.getPet(0).hp - before;
            CHECK(healed >= 300 && healed <= 500,
                  "T2332.b 1132 无提升：回复∈[300,500]（实测 " + std::to_string(healed) + "）");
        }
        std::cout << "  [T2332] 1132 随机带回复+对照 通过\n";
    }

    std::cout << "scenario_2330 ALL PASS" << std::endl;
    return 0;
}
