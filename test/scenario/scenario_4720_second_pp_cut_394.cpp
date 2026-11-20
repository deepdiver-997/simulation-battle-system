#include <utils/sbs_rand.h>
// 场景 4720 —— K316 后手 PP 压制族（394，soul_lib/k316_second_pp_cut.cpp）
//（2026-10-21，worktree ../agents/k316；号段 4720-4729 → T4720-T4722 三块）
//
// 断言口径（394「每回合若自身后出手则有50%的概率使对手所有PP降低1点（boss无效）」）：
//   - T4720 **后手门开 + 全技能PP各-1**：印主是**后手**（速度慢）→ 对手每个技能的
//     pp 都 -1（钳 0）。
//   - T4721 **先手门闭**：印主是**先手**（速度快）→ 对手 pp 不变。
//   - T4722 **概率档（统计带）**：40 局后手局，PP 被削的局数 ∈ [12, 28]
//     （Binomial(40, 0.5) 的 μ=20、σ=√10≈3.16 → ±2.5σ ≈ [12,28]）。
//     ⚠️ **概率效果必须多种子/多局采样**（K267 645 / K313 686 教训：单次抽样不能
//     证明概率档位）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {50,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **先/后手用速度控制**：印主 speed=100 / 对手 speed=300 → 印主是后手；
//   反过来（印主 300 / 对手 100）→ 印主是先手。引擎 is_second_mover 助手按
//   先手权比较判定（同速则按其它规则）。
// ⚠️ **PP 会在回合恢复**：引擎每回合恢复 PP → 故断言读**当回合 ROUND_END 之后**
//   的 pp 值（效果与恢复同在回合末，顺序由引擎定；场景用"对手 pp 是否都 < 初始"
//   的整体判据，避开单点顺序敏感）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：filler
constexpr int kMaxHp = 99999;

SoulMark make_mark() {
    // DB args 原样 [50,1]：db[0]=50（"50%"）、db[1]=1（"降低1点"）——双参自证。
    SoulMark sm(394, "K316 魂印", "test", EffectArgs(std::vector<int>{50, 1}));
    sm.monster_id = 2817;
    return sm;
}

// 印主：speed 决定先/后手。
ElfPet mark_pet(int speed) {
    ElfPet p = make_pet(1, "大地之神·卡修斯", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手所有技能的 pp 最小值（观察"是否被削"）。
int foe_min_pp(BattleContext& ctx) {
    int mn = 999;
    for (const Skills& s : ctx.getPet(1).skills) {
        if (s.pp < mn) {
            mn = s.pp;
        }
    }
    return mn;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4720 后手门开：对手所有技能 PP 各 -1 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),  // 印主慢 → 后手
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 先把对手 PP 压到 5（低于满 PP，让"被削"可观测；PP 会回补，故看相对变化）。
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = 5;
        }
        const int pp_before = foe_min_pp(ctx);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int pp_after = foe_min_pp(ctx);
        // 50% 档：单局可能触发也可能不触发 → 判据取"要么全被削、要么全未削"。
        // ⚠️ **必须用"全技能"判据**（不是"任一技能"）——官方"使对手**所有**PP降低
        //   1点"= 每个技能各降 1 点；用"任一"判据时"只降 1 个技能"的实现也能通过
        //   （变异测试 MUT2 实测 exit=0 仍绿）。
        int cut_count = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp < 5) {
                ++cut_count;
            }
        }
        const int total = static_cast<int>(ctx.getPet(1).skills.size());
        CHECK(cut_count == 0 || cut_count == total,
              "T4720 394：后手 + 50% 概率 → 对手**所有**技能 PP 各 -1（被削技能数 "
                  + std::to_string(cut_count) + "/" + std::to_string(total)
                  + "，最小 pp " + std::to_string(pp_before) + " → "
                  + std::to_string(pp_after) + "）");
        std::cout << "  [T4720] 后手门开 通过\n";
    }

    // ══ T4721 先手门闭：对手 PP 不变 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/300)),  // 印主快 → 先手
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = 5;
        }
        bool any_cut = false;
        for (int r = 0; r < 3; ++r) {   // 多跑几回合，覆盖 50% 的随机性
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            for (const Skills& s : ctx.getPet(1).skills) {
                if (s.pp < 5) {
                    any_cut = true;
                }
            }
        }
        CHECK(!any_cut,
              "T4721 394：自身是**先手** → 门闭，对手 PP 全程不降（tips'若自身后出手'）");
        std::cout << "  [T4721] 先手门闭对照 通过\n";
    }

    // ══ T4722 概率统计带：40 局后手局，被削局数 ∈ [12,28] ══
    {
        int cut_rounds = 0;
        const int rounds = 40;
        for (int i = 0; i < rounds; ++i) {
            ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),
                             make_party(foe_pet(/*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (Skills& s : ctx.getPet(1).skills) {
                s.pp = 5;
            }
            sbs_srand(20261021 + i * 7919);
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            for (const Skills& s : ctx.getPet(1).skills) {
                if (s.pp < 5) {
                    ++cut_rounds;
                    break;
                }
            }
        }
        CHECK(cut_rounds >= 12 && cut_rounds <= 28,
              "T4722 394：40 局后手局，PP 被削局数落 Binomial(40,0.5) 带 [12,28]，实测 "
                  + std::to_string(cut_rounds));
        std::cout << "  [T4722] 概率统计带 通过 (" << cut_rounds << "/" << rounds << ")\n";
    }

    std::cout << "scenario_4720 PASS (K316 后手 PP 压制族)" << std::endl;
    return 0;
}
