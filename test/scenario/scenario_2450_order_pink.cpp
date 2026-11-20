// 场景 2450 —— J150 先后手附粉族（932/815/853，moves_lib/j_order_pink.cpp）
//（2026-10-04，worktree ../agents/j150；号段 2450-2459）
//
// 断言口径（粉=面板/体力比例，精确可断；red=resolvedDamage.final）：
//   - 932 先手攻击面板粉（焚天之刺 [50]，atk=1000 → +500）：
//       (a) 先手 loss==red+500；(b) 后手对照 loss==red。
//   - 815 后手当前体力粉（疯决乱拳冲 [30]）：R1 挨刀后 R2 我后手攻 →
//       delta==red+hp×30%（hp=R1 末快照，R2 对手 NONE 无伤害）；先手对照 delta==red。
//       （⚠️ 对手 NONE 按先手处理——正好构成我的后手门成立条件，J143 教训反向利用。）
//   - 853 累积粉（王·毁灭星辰击 [180,760,853]|[?,20,10,40]，base=max 5000+速 300=5300）：
//       R1 ×20%=1060；R3 ×30%=1590；R5 ×40%=2120；轮差==red+pink。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kFenTian = 30082;   // 焚天之刺 [932]|[50]（物）
constexpr int kFengJue = 19617;   // 疯决乱拳冲 [815]|[30]（物）
constexpr int kWangHui = 18331;   // 王·毁灭星辰击 [180,760,853]（物160）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "先后手粉", skills, speed, /*atk=*/1000, 5000);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2450 932 先手攻击面板粉 ══
    {
        {   // (a) 先手 → +500
            ScenarioBattle b(make_party(self_pet({kFenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red + 500, "T2450.a 932 先手：loss==red+500（loss=" +
                                          std::to_string(loss) + " red=" +
                                          std::to_string(red) + "）");
        }
        {   // (b) 后手对照 → 无粉
            ScenarioBattle b(make_party(self_pet({kFenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red, "T2450.b 932 后手对照：loss==red");
        }
        std::cout << "  [T2450] 932 先手攻击面板粉+对照 通过\n";
    }

    // ══ T2451 815 后手当前体力粉 ══
    {
        {   // (a) R1 挨刀；R2 对手 NONE（=先手）、我后手攻 → +hp×30%
            ScenarioBattle b(make_party(self_pet({kFengJue, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1 削血（对手真攻击，pri=0 拍打 atk 由对手数值定——这里不关心量）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_after_r1 = ctx.getPet(0).hp;
            CHECK(hp_after_r1 < 5000, "T2451.a 前置：缺血（" +
                                          std::to_string(hp_after_r1) + "）");
            // R2：对手 NONE（按先手处理）→ 我后手
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red + hp_after_r1 * 30 / 100,
                     "T2451.a 815 后手：loss==red+hp×30%（loss=" + std::to_string(loss) +
                         " red=" + std::to_string(red) + " hp=" +
                         std::to_string(hp_after_r1) + "）");
        }
        {   // (b) 先手对照 → 无粉
            ScenarioBattle b(make_party(self_pet({kFengJue, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 400)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red, "T2451.b 815 先手对照：loss==red");
        }
        std::cout << "  [T2451] 815 后手当前体力粉+对照 通过\n";
    }

    // ══ T2452 853 累积粉（base=5300）══
    {
        ScenarioBattle b(make_party(self_pet({kWangHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = 999999 - ctx.getPet(1).hp;
        for (int round = 1; round <= 5; round += 2) {
            const int expect_pct = std::min(20 + (round - 1) / 2 * 10, 40);
            const int expect_pink = 5300 * expect_pct / 100;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int delta = loss - prev;
            prev = loss;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(delta, red + expect_pink,
                     "T2452 R" + std::to_string(round) + "：粉=" +
                         std::to_string(expect_pink) + "（delta=" + std::to_string(delta) +
                         " red=" + std::to_string(red) + "）");
            if (round + 1 <= 5) {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
        }
        std::cout << "  [T2452] 853 累积粉 20→30→40(钳) 通过\n";
    }

    std::cout << "scenario_2450 ALL PASS" << std::endl;
    return 0;
}
