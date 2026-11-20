// 场景 2190 —— J125 回复族二（407/404/541，moves_lib/j_cond_heal2.cpp）
//（2026-10-04，worktree ../agents/j125；号段 2190-2199）
//
// 断言口径：
//   - 407 下回合起每回合特攻+1（神怒焰焚天 15780 [2,1,3]，属技；官方码 2=特攻 →
//     引擎槽 1）：R1 施放当回合不加；R2/R3/R4 各 +1；R5 窗过期恒 3。
//   - 404 恢复双方（一番茄调味 28466 纯链属）：我挨一刀（-794..934）后施放 → 我满血、
//     对手恒满血。
//   - 541 低伤回体（炼狱冥火诀 15159 [300,200]，特150）：
//       (a) atk=1（红 ~2<300）→ 施放回合回 +200；
//       (b) atk=1000（红 ≈1125+ ≥300）→ 不回。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kShenNu = 15780;    // 神怒焰焚天 [407]|[2,1,3]（特160 属？——cat=1 物160）
constexpr int kFanQie = 28466;    // 一番茄调味 [404]（属）
constexpr int kLianYu = 15159;    // 炼狱冥火诀 [541]|[300,200]（特150）
constexpr int kJiGuang = 19011;   // 激光刀（foe 削血刀）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int hp = 2000) {
    ElfPet p = make_pet(1, "回复族二", skills, /*speed=*/300, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, 999999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2190 407 下回合起每回合特攻+1（持续 3 回合）══
    {
        ScenarioBattle b(make_party(self_pet({kShenNu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T2190 R1：当回合不加（下回合起）");
        for (int r = 2; r <= 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][1], r - 1,
                     "T2190 R" + std::to_string(r) + "：特攻+" + std::to_string(r - 1));
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][1], 3, "T2190 R5：窗过期恒 3");
        std::cout << "  [T2190] 407 下回合起每回合特攻+1 通过\n";
    }

    // ══ T2191 404 恢复双方 ══
    {
        ScenarioBattle b(make_party(self_pet({kFanQie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1600)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我挨一刀
        b.drive();
        const int after_hit = ctx.getPet(0).hp;
        CHECK(after_hit < 2000 && after_hit > 0,
              "T2191 前置：我缺血（实测 " + std::to_string(after_hit) + "）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 一番茄调味
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 2000, "T2191 404：我回满");
        CHECK_EQ(ctx.getPet(1).hp, 999999, "T2191 404：对手恒满血");
        std::cout << "  [T2191] 404 恢复双方 通过\n";
    }

    // ══ T2192 541 低伤回体 ══
    {
        {   // (a) atk=1：红 <300 → +200
            ScenarioBattle b(make_party(self_pet({kLianYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*atk=*/1)),
                             make_party(foe_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1600)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int after_hit = ctx.getPet(0).hp;
            CHECK(after_hit < 2000, "T2192.a 前置：我缺血");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 炼狱冥火诀
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, after_hit + 200,
                     "T2192.a 541 低伤：恰 +200（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
        }
        {   // (b) atk=1000：红 ≥300 → 不回
            ScenarioBattle b(make_party(self_pet({kLianYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*atk=*/1000, /*hp=*/99999)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK(red >= 300, "T2192.b 前置：红过阈值（red=" + std::to_string(red) + "）");
            CHECK_EQ(loss, red, "T2192.b 541 高伤：不回体（loss==red）");
        }
        std::cout << "  [T2192] 541 低伤回体+对照 通过\n";
    }

    std::cout << "scenario_2190 ALL PASS" << std::endl;
    return 0;
}
