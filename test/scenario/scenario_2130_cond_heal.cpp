// 场景 2130 —— J119 条件恢复族（704/1635，moves_lib/j_cond_heal.cpp）
//（2026-10-03，worktree ../agents/j119；号段 2130-2139）
//
// 断言口径：
//   - 704 低体回满门（烈阳瞬闪 32611 [250,250]）：
//       (a) 先挨一刀（foe 激光刀 pow80 atk=1600 → 红 794-934）→ 我 hp 66-206 < 250 →
//           施放后满血（替换读法，非 +250）；
//       (b) 轻削一刀（hp 约 684-716 仍>250）→ 施放恰 +250（else 分支）。
//   - 1635 延迟回满（誓言之约 27399 [200,5]，属）：
//       (a) 挨一刀（-680..800）→ 施放恰 +200；R2-R5 未到期不回满（foe 全程 pass，
//           断言 R5 末仍缺血）；R6 回合开始回满（hp==max）。
//       (b) 满血施放对照：立刻恢复无可恢复（hp 恒满），到期回满无变化（恒满）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打（对照刀）
constexpr int kLieYang = 32611;   // 烈阳瞬闪 [704]|[250,250]（光系物攻 140）
constexpr int kShiYue  = 27399;   // 誓言之约 [1635]|[200,5]（属）
constexpr int kJiGuang = 19011;   // 激光刀 [750] 物80（foe 削血刀，pri=3 无碍）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 1000) {
    ElfPet p = make_pet(1, "恢复族", skills, /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2130 704 低体回满门 ══
    {
        {   // (a) 先挨一刀（hp 80-218 < 250）→ 施放回满
            ScenarioBattle b(make_party(self_pet({kLieYang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1600)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 激光刀削血
            b.drive();
            const int after_hit = ctx.getPet(0).hp;
            CHECK(after_hit < 250 && after_hit > 0,
                  "T2130.a 前置：hp∈(0,250)（实测 " + std::to_string(after_hit) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 烈阳瞬闪
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1000,
                     "T2130.a 704 低体：直接回满（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
        }
        {   // (b) 轻削一刀（hp 680-728，仍>250）→ 施放恰 +250
            ScenarioBattle b(make_party(self_pet({kLieYang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打轻削
            b.drive();
            const int after_hit = ctx.getPet(0).hp;
            CHECK(after_hit > 250 && after_hit < 1000,
                  "T2130.b 前置：hp∈(250,1000)（实测 " + std::to_string(after_hit) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 烈阳瞬闪
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, after_hit + 250,
                     "T2130.b 704 常规：恰 +250（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
        }
        std::cout << "  [T2130] 704 低体回满门+对照 通过\n";
    }

    // ══ T2131 1635 延迟回满（5回合后）══
    {
        {   // (a) 挨一刀 → 施放恰 +200 → R5 末仍缺血 → R6 回合开始回满
            ScenarioBattle b(make_party(self_pet({kShiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kJiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 削血（-680..800）
            b.drive();
            const int after_hit = ctx.getPet(0).hp;
            CHECK(after_hit >= 200 && after_hit < 1000,
                  "T2131.a 前置：缺血可恢复（实测 " + std::to_string(after_hit) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 誓言之约
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, after_hit + 200,
                     "T2131.a 1635 立刻恢复恰 +200（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
            for (int r = 2; r <= 5; ++r) {   // R2-R5：未到期，foe pass，血量不变
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                CHECK_EQ(ctx.getPet(0).hp, after_hit + 200,
                         "T2131.a R" + std::to_string(r) + " 未到期不回满");
            }
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R6：到期
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1000,
                     "T2131.a R6 回合开始回满（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
        }
        {   // (b) 满血施放对照：立刻恢复无变化、到期回满无变化
            ScenarioBattle b(make_party(self_pet({kShiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1000, "T2131.b 满血施放：无变化");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1000, "T2131.b 到期：恒满无变化");
        }
        std::cout << "  [T2131] 1635 延迟回满+对照 通过\n";
    }

    std::cout << "scenario_2130 ALL PASS" << std::endl;
    return 0;
}
