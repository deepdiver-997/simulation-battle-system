// 场景 2160 —— J122 面板粉伤族二（534/670，moves_lib/j_panel_pink2.cpp）
//（2026-10-03，worktree ../agents/j122；号段 2160-2169）
//
// 断言口径（粉伤不受 85-100% 红伤浮动影响，精确可断）：
//   - 534（神力碎土拳 15026 纯链物150）：双防面板和（100+100）×10% = 恰 20：
//       loss == red + 20（red=resolvedDamage.final）。
//   - 670 物攻版（神威破八荒 16579 [3,0,0,15]，atk=1000 → 每回合 +150）：
//       R1 loss==red+150；R2/R3 各再 +150；R4 窗过期不再涨。
//   - 670 特攻版（闪芒之浴 26915 [3,0,2,50]，属技无红伤）：链上 1001 全属性+1 当回合
//       生效 → 特攻面板 1000×1.5=1500，每回合粉 = 750（非 500——链激活活例）；R4 不再涨。
//       （1067 恢复转吸血：我满血 heal 钳 0 → 无转换，不产伤。）
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打（填充）
constexpr int kSuiTu  = 15026;    // 神力碎土拳 [534] 物150
constexpr int kShenWei = 16579;   // 神威破八荒 [670]|[3,0,0,15] 特150
constexpr int kShanMang = 26915;  // 闪芒之浴 [1001,1067,670]|[?, ?, 3,0,2,50] 属

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1000) {
    ElfPet p = make_pet(1, "面板粉", skills, /*speed=*/300, atk, 5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2160 534 双防面板和 10% 粉 ══
    {
        ScenarioBattle b(make_party(self_pet({kSuiTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int loss = 999999 - ctx.getPet(1).hp;
        const int red = ctx.resolvedDamage.final;
        CHECK(red > 0, "T2160 前置：红伤落地（red=" + std::to_string(red) + "）");
        CHECK_EQ(loss, red + 20,
                 "T2160 534：loss==red+20（loss=" + std::to_string(loss) + " red=" +
                     std::to_string(red) + "）");
        std::cout << "  [T2160] 534 双防面板粉 通过\n";
    }

    // ══ T2161 670 物攻版每回合窗（3 回合 ×15%）══
    {
        ScenarioBattle b(make_party(self_pet({kShenWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = 999999 - ctx.getPet(1).hp;
        for (int r = 1; r <= 4; ++r) {
            if (r == 1) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            } else {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            }
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int delta = loss - prev;
            prev = loss;
            if (r == 1) {
                const int red = ctx.resolvedDamage.final;
                CHECK_EQ(delta, red + 150,
                         "T2161 R1：loss==red+150（delta=" + std::to_string(delta) + "）");
            } else if (r <= 3) {
                CHECK_EQ(delta, 150, "T2161 R" + std::to_string(r) + "：+150（实测 " +
                                         std::to_string(delta) + "）");
            } else {
                CHECK_EQ(delta, 0, "T2161 R4 窗过期：不再涨（实测 " +
                                       std::to_string(delta) + "）");
            }
        }
        std::cout << "  [T2161] 670 物攻版 3 回合窗 通过\n";
    }

    // ══ T2162 670 特攻版（{2}=2，属技纯粉）══
    {
        ScenarioBattle b(make_party(self_pet({kShanMang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = 999999 - ctx.getPet(1).hp;
        for (int r = 1; r <= 4; ++r) {
            if (r == 1) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            } else {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            }
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int delta = loss - prev;
            prev = loss;
            if (r <= 3) {
                CHECK_EQ(delta, 750, "T2162 R" + std::to_string(r) + "：面板1500×50%=+750（实测 " +
                                         std::to_string(delta) + "）");
            } else {
                CHECK_EQ(delta, 0, "T2162 R4 窗过期：不再涨（实测 " +
                                       std::to_string(delta) + "）");
            }
        }
        std::cout << "  [T2162] 670 特攻版 通过\n";
    }

    std::cout << "scenario_2160 ALL PASS" << std::endl;
    return 0;
}
