// 场景 5130 —— J191 固伤递增族（1480，moves_lib/j_fixed_ramp_1480.cpp）
//（2026-10-22，worktree ../agents/j191；号段 5130-5139）
//
// 断言口径：
//   - 1480（极电闪耀 33748 纯链 [1480]|[150,100,350]，特 140）：同一次上场连续 4 回合
//     使用 → 固定伤害序列 150 / 250 / 350 / 350（第 3 次触顶后封顶）。
//   - 载体是攻击技，主伤害会叠加：自身 atk=1（实测主伤 1-2 点，速查 §五），
//     逐回合掉血带 = 固伤 + [1,2]，故断言 [150,155] / [250,255] / [350,355] /
//     [350,355]（相邻档差 100，远大于主伤波动，档位可分辨）。
//   - 用回合内 hp 前后快照差分（不写"等级和/累计值"），目标 99999 血不会死亡。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物 40（填充刀）
constexpr int kJiDian = 33748;  // [1480]|[150,100,350]（固伤 150 起、每次 +100、封顶 350）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_attacker() {
    ElfPet p = make_pet(1, "己方", {kJiDian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5130 1480 连续使用 → 固伤递增并封顶 ══
    {
        ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect_lo[4] = {150, 250, 350, 350};
        const int expect_hi[4] = {155, 255, 355, 355};
        int prev_hp = ctx.getPet(1).hp;
        for (int r = 0; r < 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int hp = ctx.getPet(1).hp;
            const int loss = prev_hp - hp;   // 固伤 + 主伤(1-2)
            CHECK(loss >= expect_lo[r] && loss <= expect_hi[r],
                  "T5130 第 " + std::to_string(r + 1) + " 次使用掉血=" +
                      std::to_string(loss) + "，期望 [" + std::to_string(expect_lo[r]) +
                      "," + std::to_string(expect_hi[r]) + "]（固伤档位）");
            prev_hp = hp;
        }
        std::cout << "  [T5130] 1480 固伤 150→250→350→350 通过\n";
    }

    std::cout << "scenario_5130 PASS (J191 固伤递增族)" << std::endl;
    return 0;
}
