#include <utils/sbs_rand.h>
// 场景 3170 —— K185 对手强化态双分支族（1148，soul_lib/k185_opp_boost_branch.cpp）
//（2026-10-07，worktree ../agents/k185，看板段 3170-3179 → T3170-T3173）
//
// 断言口径：
//   - T3170 无强化分支·进化行 [100,1]（对照对局对同 seed）：增伤 ×2 走 AMP 整数式
//     final×200/100 → 失血恰 = 2×对照（branch 旗与增伤均零掷点，序列一致）。
//   - T3171 无强化分支·本体行 [60,1]：失血 ∈ [1.5,1.7]×对照（60% 增伤 ±浮动手带）。
//   - T3172 有强化分支：对手预置 +2 攻 → 回合开始评"有强化"置旗 → 持印者攻击后
//     对手强化被清（ability_level 回 0）；同 seed 对照（无印）强化保留、失血相等
//     （消除在伤害落地后、不掷骰 → 两局伤害序列一致，验证有强化分支**不吃增伤**）。
//   - T3173 分支翻转：R1 对手有强化（被清）→ R3 对手无强化，攻击失血 ≈ 2×基准
//     （进化行 100% 增伤；两局 rand 流不对齐，用 [1.7,2.3]× 手带分离"有/无增伤"）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击载体
constexpr int kMaxHp = 99999;

template <int MarkId>
ElfPet mark_pet(EffectArgs mark_args, int speed) {
    ElfPet p = make_pet(1, "虞姬", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(MarkId, "对手强化态魂印", "",
                                               std::move(mark_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_me(int speed) {
    ElfPet p = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_actions(ScenarioBattle& b, bool attack) {
    const auto act = attack ? BattleFsm::ActionType::SELECT_SKILL
                            : BattleFsm::ActionType::NONE;
    b.submit_operation(0, act, 0);
    b.submit_operation(1, act, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3170/T3171 无强化分支：进化行 ×2 精确、本体行 60% 手带 ══
    {
        const int amps[2] = {100, 60};
        for (int row = 0; row < 2; ++row) {
            int marked_loss = -1;
            int plain_loss = -1;
            for (int side = 0; side < 2; ++side) {
                auto me = side == 0
                    ? mark_pet<1148>(EffectArgs(std::vector<int>{amps[row], 1}),
                                     /*speed=*/300)
                    : plain_me(/*speed=*/300);
                ScenarioBattle b(make_party(std::move(me)),
                                 make_party(plain_foe(/*speed=*/100)));
                b.ctx().seed_rng(7);
                b.start();
                round_actions(b, /*attack=*/true);
                const int loss = kMaxHp - b.ctx().getPet(1).hp;
                if (side == 0) {
                    marked_loss = loss;
                } else {
                    plain_loss = loss;
                }
            }
            CHECK(marked_loss > plain_loss, "T3170 前置：无强化分支增伤生效");
            if (amps[row] == 100) {
                CHECK_EQ(marked_loss, plain_loss * 2,
                         "T3170 1148：进化行增伤100% → 失血恰 2×对照");
            } else {
                CHECK(marked_loss * 10 >= plain_loss * 15
                          && marked_loss * 10 <= plain_loss * 17,
                      "T3171 1148：本体行增伤60% → 失血 ∈ [1.5,1.7]×对照");
            }
            std::cout << "  [T317" << row << "] 增伤" << amps[row]
                      << "% 通过 (marked=" << marked_loss
                      << " plain=" << plain_loss << ")\n";
        }
    }

    // ══ T3172 有强化分支：攻击消强、不增伤 ══
    {
        int marked_loss = -1;
        int plain_loss = -1;
        for (int side = 0; side < 2; ++side) {
            auto me = side == 0
                ? mark_pet<1148>(EffectArgs(std::vector<int>{100, 1}), /*speed=*/300)
                : plain_me(/*speed=*/300);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_foe(/*speed=*/100)));
            b.ctx().seed_rng(7);
            b.start();
            core_api().stat_change(&b.ctx(), /*target=*/1, /*stat=*/0, /*delta=*/2);
            round_actions(b, /*attack=*/true);
            if (side == 0) {
                marked_loss = kMaxHp - b.ctx().getPet(1).hp;
                CHECK_EQ(b.ctx().ability_level(1, 0), 0,
                         "T3172 1148：有强化分支 → 持印者攻击消除对手 +2 强化");
            } else {
                plain_loss = kMaxHp - b.ctx().getPet(1).hp;
                CHECK_EQ(b.ctx().ability_level(1, 0), 2,
                         "T3172 对照：无印 → 对手强化保留");
            }
        }
        CHECK_EQ(marked_loss, plain_loss,
                 "T3172 1148：有强化分支不吃增伤（同 seed 同伤害）");
        std::cout << "  [T3172] 有强化消强 通过 (loss=" << marked_loss << ")\n";
    }

    // ══ T3173 分支翻转：R1 消强 → R3 无强化走增伤 ══
    {
        ScenarioBattle b(make_party(mark_pet<1148>(EffectArgs(std::vector<int>{100, 1}),
                                                   /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        b.ctx().seed_rng(7);
        b.start();
        core_api().stat_change(&b.ctx(), 1, 0, 2);
        round_actions(b, /*attack=*/true);    // R1：有强化 → 消强
        CHECK_EQ(b.ctx().ability_level(1, 0), 0, "T3173 前置：R1 强化被清");
        round_actions(b, /*attack=*/false);   // R2 双 NONE
        const int before = b.ctx().getPet(1).hp;
        round_actions(b, /*attack=*/true);    // R3：对手已无强化 → 增伤分支
        const int loss_r3 = before - b.ctx().getPet(1).hp;
        // 基准：单回合无印对照（rand 流不对齐，用 [1.7,2.3]× 手带分离增伤有无）
        ScenarioBattle c(make_party(plain_me(/*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        c.ctx().seed_rng(7);
        c.start();
        round_actions(c, /*attack=*/true);
        const int base = kMaxHp - c.ctx().getPet(1).hp;
        CHECK(loss_r3 * 10 >= base * 17 && loss_r3 * 10 <= base * 23,
              "T3173 1148：翻转后无强化回合增伤生效（≈2×基准手带）");
        std::cout << "  [T3173] 分支翻转 通过 (r3=" << loss_r3 << " base=" << base
                  << ")\n";
    }

    std::cout << "scenario_3170 PASS (K185 对手强化态双分支族)" << std::endl;
    return 0;
}
