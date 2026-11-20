// 场景 5030 —— J188 高伤回血族（1341，moves_lib/j_high_damage_heal.cpp）
//（2026-10-22，worktree ../agents/j188；号段 5030-5039）
//
// 断言口径：
//   - 1341（菇菇发之梦 32505 纯链 [1341]|[300,3]）：攻击伤害 final > 300 → 自身回 max/3。
//     自身体力开局压到 2000（max 5000，避免 clamp）：atk=2000 打 def=100 的靶子，
//     raw≈[578,680]（[87,102] 锚 × atk/300，留足 >300 裕量）→ 回 5000/3=1666 → 3666。
//   - 对照：atk=100（raw≈[29,34] < 300）→ 不回血，体力保持 2000。
//   - 阈值门用绝对量断言（非"与对照相等"）；攻击走真实技能结算（DETECT 相位）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kGuGu    = 32505;  // [1341]|[300,3]（伤害 >300 → 回 max/3）
constexpr int kMaxHp   = 5000;
constexpr int kHealAmt = kMaxHp / 3;   // 1666

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_attacker(int atk) {
    ElfPet p = make_pet(1, "己方", {kGuGu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/kMaxHp);
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

    // ══ T5030 1341 伤害 > 阈值 → 回 max/{1} ══
    {
        {   // (a) atk=2000：final≈[578,680] > 300 → 回 1666
            ScenarioBattle b(make_party(mk_attacker(/*atk=*/2000)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;   // 压低起始体力，回血可见
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 2000 + kHealAmt,
                     "T5030.a 1341：伤害 >300 → 回 max/3=1666（实测 hp=" +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        {   // (b) 对照 atk=100：final≈[29,34] <= 300 → 不回
            ScenarioBattle b(make_party(mk_attacker(/*atk=*/100)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 2000,
                     "T5030.b 1341 对照：伤害 <=300 → 不回血（实测 hp=" +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T5030] 1341 高伤回血 + 阈值对照 通过\n";
    }

    std::cout << "scenario_5030 PASS (J188 高伤回血族)" << std::endl;
    return 0;
}
