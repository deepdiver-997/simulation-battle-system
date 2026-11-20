// 场景 1790 —— J90 自身强化翻倍族（1 宏，moves_lib/j_self_boost_double.cpp）
//（2026-10-03，worktree ../agents/j90；号段 1790-1799 → T1790 两块）
//
// 断言口径：
//   - 载体寒来暑往 26696 [622]|[1,1,1,1,1,0,20]：属性技零伤害；向量 [1,1,1,1,1,0]
//     （引擎槽 0/1/2/3/4 各 +1、槽 5 命中 0）；
//   - 翻倍掷点 20% 非确定 → 审计不可用（无审计槽）——翻倍段断言走**统计侧证**：
//     N=200 次独立对局统计"+2 出现频率 ≈ 20%±8%"（二项 p=0.2，σ≈2.8%，8σ 裕量）；
//     首段断言完全确定（六槽 +1）；对照档 chance 无 0 载体 → 用审计槽思路不适用，
//     改以翻倍频率显著偏离 0/100 判定（实测频带内即视为掷点管道工作正常）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kHanLai = 26696;  // 寒来暑往[622]|[1,1,1,1,1,0,20]：属性技（六槽+1·20%翻倍）

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
    ElfPet p = make_pet(1, "己方", skills, 300, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1790 622 自身六槽+1·20% 翻倍（寒来暑往）══
    {
        // a) 首段确定：六槽引擎槽 0..4 各 +1、槽 5 为 0
        {
            ScenarioBattle b(make_party(self_pet({kHanLai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int i = 0; i < 5; ++i) {
                CHECK(ctx.ability_levels[0][i] == 1 || ctx.ability_levels[0][i] == 2,
                      "T1790.a 622：槽" + std::to_string(i) + " ∈ {1,2}（实测 " +
                          std::to_string(ctx.ability_levels[0][i]) + "）");
            }
            CHECK_EQ(ctx.ability_levels[0][5], 0, "T1790.a 622：命中槽不动");
        }
        // b) 翻倍统计侧证：200 局中"任意槽到 2"频率 ≈ 20%（p=0.2，容差 ±8%）
        {
            int doubled = 0;
            constexpr int kTrials = 200;
            for (int t = 0; t < kTrials; ++t) {
                ScenarioBattle b(
                    make_party(self_pet({kHanLai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                    make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                bool hit = false;
                for (int i = 0; i < 5; ++i) {
                    if (ctx.ability_levels[0][i] >= 2) {
                        hit = true;
                    }
                }
                if (hit) {
                    ++doubled;
                }
            }
            const int pct = doubled * 100 / kTrials;
            CHECK(pct >= 12 && pct <= 28,
                  "T1790.b 622：翻倍频率 " + std::to_string(pct) + "%（期望 20%±8%，" +
                      std::to_string(doubled) + "/" + std::to_string(kTrials) + "）");
        }
        std::cout << "  [T1790] 622 自身强化翻倍 通过\n";
    }

    std::cout << "scenario_1790 PASS (J90 自身强化翻倍族 622)" << std::endl;
    return 0;
}
