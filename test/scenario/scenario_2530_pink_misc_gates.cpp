// 场景 2530 —— J168 粉体杂门族（992/1461/1205，moves_lib/j_pink_misc_gates.cpp）
//（2026-10-04，worktree ../agents/j168；号段 2530-2539）
//
// 断言口径：
//   - 992 窗内骑乘粉（覆海腾云翻 34935 [992,1415]|[3,30] 物0，我 hp 恒 5000 → 粉=1500）：
//       R1 施放（当击不吃）；R2/R3 我拍打 → 对手 loss 轮差 = red+1500；R4 窗过期 = red。
//   - 1461 回体+等量粉（判罚秘令 27864 [3] 属，max=4000）：
//       (a) 对手 hp 高 → healed=缺口（顶格）→ 我回满 4000、对手掉 healed 等量；
//       (b) 我 hp>对手 → 无粉（heal 照常）。
//   - 1205 后手全属性（灵魂抽离 31310 [1] 特）：(a) 后手 → 六槽 +1；
//     (b) 先手对照 → 不加。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kFuHai  = 34935;    // 覆海腾云翻 [992,1415]|[3,30]（物0）
constexpr int kPanJue = 27864;    // 判罚秘令 [1461]|[3]（属）
constexpr int kLingHun = 31310;   // 灵魂抽离 [1205]|[1]（特）
constexpr int kJiGuangDao = 19011; // 激光刀（foe 削血刀）
constexpr int kHunDunDao  = 17201; // 混沌利爪（foe 大威力削血）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "粉体门", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 999999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2530 992 窗内骑乘粉 ══
    {
        ScenarioBattle b(make_party(self_pet({kFuHai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放（当击不吃）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        int prev = 999999 - ctx.getPet(1).hp;
        for (int r = 2; r <= 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打（攻击技）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 999999 - ctx.getPet(1).hp;
            const int delta = loss - prev;
            prev = loss;
            const int red = ctx.resolvedDamage.final;
            if (r <= 3) {
                CHECK_EQ(delta, red + 1500,
                         "T2530 R" + std::to_string(r) + "：delta==red+1500（delta=" +
                             std::to_string(delta) + " red=" + std::to_string(red) + "）");
            } else {
                CHECK_EQ(delta, red, "T2530 R4 窗过期：delta==red");
            }
        }
        std::cout << "  [T2530] 992 窗内骑乘粉 通过\n";
    }

    // ══ T2531 1461 回体+等量粉 ══
    {
        {   // (a) 我 hp<对手 → healed=缺口（顶格）、对手掉等量
            ScenarioBattle b(make_party(self_pet({kPanJue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, 4000)),
                             make_party(foe_pet({kJiGuangDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, /*atk=*/1100, /*hp=*/999999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R1 挨刀造缺口
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            const int healed_expect = 4000 - before;   // 缺口 < 1333 → healed=缺口（顶格）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 判罚秘令
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4000,
                     "T2531.a 1461：我回满 4000（实测 " + std::to_string(ctx.getPet(0).hp) +
                         "）");
            const int foe_loss = 999999 - ctx.getPet(1).hp;
            CHECK(foe_loss >= healed_expect, "T2531.a 1461：对手掉等量粉（foe_loss=" +
                                                 std::to_string(foe_loss) + " healed=" +
                                                 std::to_string(healed_expect) + "）");
        }
        {   // (b) 我 hp>对手 → 无粉（heal 照常）
            ScenarioBattle b(make_party(self_pet({kPanJue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, 8000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, 2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我满血施放
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, 2000, "T2531.b 1461 对照：无粉");
            std::cout << "  [T2531.b] 无粉对照 通过\n";
        }
        std::cout << "  [T2531] 1461 回体+等量粉+对照 通过\n";
    }

    // ══ T2532 1205 后手全属性 ══
    {
        {   // (a) 后手 → 六槽 +1
            ScenarioBattle b(make_party(self_pet({kLingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int ups = 0;
            for (int i = 0; i < 6; ++i) {
                if (ctx.ability_levels[0][i] == 1) {
                    ++ups;
                }
            }
            CHECK_EQ(ups, 6, "T2532.a 1205 后手：六槽 +1（实测 " + std::to_string(ups) + "）");
        }
        {   // (b) 先手对照 → 不加
            ScenarioBattle b(make_party(self_pet({kLingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 400)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int ups = 0;
            for (int i = 0; i < 6; ++i) {
                if (ctx.ability_levels[0][i] > 0) {
                    ++ups;
                }
            }
            CHECK_EQ(ups, 0, "T2532.b 1205 先手对照：不加");
        }
        std::cout << "  [T2532] 1205 后手全属性+对照 通过\n";
    }

    std::cout << "scenario_2530 ALL PASS" << std::endl;
    return 0;
}
