// 场景 2480 —— J157 受击响应族四（724，moves_lib/j_onhit_heal_chance.cpp）
//（2026-10-04，worktree ../agents/j157；号段 2480-2489）
//
// 断言口径：
//   - (a) 烈炎滚滚 36664 [4,100,3]（100% 回 1/3）确定性：R1 武装即打（对手吃红伤）；
//     R2..R5 挨四刀各回 max/4=1250（1500 满血、缺口 1000/刀 240-330×4 → 逐刀验证）。
//     前置：我 hp 压到 1500、缺口充足。
//   - (b) 窗过期：R6 挨刀不回。
//   - (c) 溯洄 28882 [4,50,1]（50% 回满）统计带：挨刀 20 场回满次数 ∈ [4,16]。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include <cstdio>

#include <string>
#include <utility>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打（foe 削血刀，atk 大）
constexpr int kLieYan = 36664;    // 烈炎滚滚 [724]|[4,100,3]（特，100% 回 1/3）
constexpr int kSuHui  = 28882;    // 溯洄 [724]|[4,50,1]（特，50% 回满）
constexpr int kHunDunDao = 17201; // 混沌利爪（物150 大威力削血）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 1500) {
    ElfPet p = make_pet(1, "受击回体", skills, /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1800) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    {   // (a)+(b) 100% 确定档（max=3000，heal=max/3=1000）：
        //     debug_step 单步到我方行动结算完（DETECT 回体已落、对手未出手）→
        //     hp 差 = 精确回体量；R2..R5 窗内各 +1000；R6 过期净负。
        //     foe：R1 idx0 混沌利爪造缺口、R2..R6 idx1 拍打@1800（-440..-620）。
        ScenarioBattle b(make_party(self_pet({kLieYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*hp=*/3000)),
                         make_party(foe_pet({kHunDunDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1 造缺口（foe 混沌利爪，我 NONE）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int gap = 3000 - ctx.getPet(0).hp;
        CHECK(gap >= 1000, "前置：缺口≥1000（实测 " + std::to_string(gap) + "）");
        auto net_of_round = [&](int my_idx, int foe_idx) {
            const int before = ctx.getPet(0).hp;
            ctx.debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_idx);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
            b.step_past(State::BATTLE_SECOND_ATTACK_DAMAGE);
            const int net = ctx.getPet(0).hp - before;   // = 回体 − 拍打伤（SECONDS 态已全落）
            ctx.debug_step_mode = false;
            b.drive();
            return net;
        };
        // R2 武装（我的 op=烈炎滚滚）；R3..R5 我的 op 换拍打（防重施叠 watcher）
        for (int r = 2; r <= 5; ++r) {
            const int net = net_of_round(r == 2 ? 0 : 1, 1);
            CHECK(net >= 380 && net <= 560,
                  "T2480.a R" + std::to_string(r) + "：净=1000−刀伤 ∈ [380,560]（实测 " +
                      std::to_string(net) + "）");
        }
        {   // R6 过期（我的 op 换拍打，避免重施叠窗）：净=−拍打伤（无回体）
            const int net = net_of_round(1, 1);
            CHECK(net >= -620 && net <= -440,
                  "T2480.b R6 窗过期：净=−刀伤 ∈ [-620,-440]（实测 " +
                      std::to_string(net) + "）");
        }
        std::cout << "  [T2480.a/b] 100% 确定档 受击回 1/3 + 窗过期 通过\n";
    }

    {   // (c) 50% 回满统计带
        int heals = 0;
        for (int t = 0; t < 20; ++t) {
            ScenarioBattle b(make_party(self_pet({kSuHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (ctx.getPet(0).hp == 1500) {   // 回满（净=−刀伤+1500 → 恰回满顶格）
                ++heals;
            }
        }
        CHECK(heals >= 4 && heals <= 16,
              "T2480.c 统计带：20 场回满 " + std::to_string(heals) + " ∉ [4,16]");
        std::cout << "  [T2480.c] 50% 回满统计带 通过（heals=" << heals << "/20）\n";
    }

    std::cout << "scenario_2480 ALL PASS" << std::endl;
    return 0;
}
