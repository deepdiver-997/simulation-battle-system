// 场景 1830 —— J94 击败PP/护盾固伤族（2 宏，moves_lib/j_defeat_pp_shield.cpp）
//（2026-10-03，worktree ../agents/j94；号段 1830-1839 → T1830-T1831 两块）
//
// 断言口径：
//   - 1286 = 两宠队伍死切驱动（953 协议）：自身技能 PP 预置低位（1/2/3），枪托打击
//     [1286]（物140）击杀 hp=1 稻草靶 → 死切停点断全 PP 回满（>10，拍打族 maxPP 25+）；
//     负例 = 对手 99999 血存活 → PP 保持低位；
//   - 1272 = 场景直呼 shield_bank_.add 预置 500 盾（priority 0/duration 0）→ 法则崩毁
//     [1272]（物120，红伤带 [173,203]）→ 对手掉 = 红伤 + 500 = [673,703]；盾被消耗
//     （total()==0）；负例 = 无盾仅红伤带；
//   - 载体双纯链零参属性面干净（attacking 载体红伤不进断言面的部分已计带）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kQiangTuo = 34419; // 枪托打击[1286]：物140（击败→PP全回）
constexpr int kFaZe   = 35827;   // 法则崩毁[1272]：物120（附加当前护盾值等量固伤）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int hp = kMaxHp, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, 100, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1830 1286 击败 → PP 全回（枪托打击）══
    {
        // a) 正例：PP 预置低位 → 击杀稻草靶 → 全回
        {
            ElfPet me = self_pet({kQiangTuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            int baseline[5];
            for (int i = 0; i < 5; ++i) {
                baseline[i] = i + 1;                  // 预置低位 1..5
                ctx.getPet(0).skills[i].pp = baseline[i];
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 击杀
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T1830.a 前置：停在死切");
            for (int i = 0; i < 5; ++i) {
                // 回满 = 超过预置基线（各技能 maxPP 不同：枪托打击 5、拍打族 25+）
                CHECK(ctx.getPet(0).skills[i].pp > baseline[i],
                      "T1830.a 1286：槽" + std::to_string(i) + " PP 回满超基线（实测 " +
                          std::to_string(ctx.getPet(0).skills[i].pp) + " > " +
                          std::to_string(baseline[i]) + "）");
            }
        }
        // b) 负例：未击败 → PP 保持低位
        {
            ScenarioBattle b(make_party(self_pet({kQiangTuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int i = 0; i < 5; ++i) {
                ctx.getPet(0).skills[i].pp = i + 1;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T1830.b 前置：对手存活");
            CHECK(ctx.getPet(0).skills[1].pp <= 3,
                  "T1830.b 1286 门：未击败 PP 不回（槽1 实测 " +
                      std::to_string(ctx.getPet(0).skills[1].pp) + "）");
        }
        std::cout << "  [T1830] 1286 击败回PP 通过\n";
    }

    // ══ T1831 1272 护盾值等量固伤（法则崩毁）══
    {
        // a) 正例：预置 500 盾 → 红伤带 + 500；盾被消耗
        {
            ScenarioBattle b(make_party(self_pet({kFaZe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/500,
                                           /*source_id=*/999999, /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 673 && foe_loss <= 703,
                  "T1831.a 1272：红伤带+盾值 500（实测 " + std::to_string(foe_loss) +
                      "，带 [673,703]）");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T1831.a 1272：盾已消耗（实测余 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        // b) 负例：无盾 → 仅红伤带
        {
            ScenarioBattle b(make_party(self_pet({kFaZe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 173 && foe_loss <= 203,
                  "T1831.b 1272 门：无盾仅红伤（实测 " + std::to_string(foe_loss) +
                      "，带 [173,203]）");
        }
        std::cout << "  [T1831] 1272 护盾固伤 通过\n";
    }

    std::cout << "scenario_1830 PASS (J94 击败PP/护盾固伤族 1286/1272)" << std::endl;
    return 0;
}
