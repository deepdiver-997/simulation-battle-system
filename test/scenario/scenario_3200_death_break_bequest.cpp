#include <utils/sbs_rand.h>
// 场景 3200 —— K188 死亡消回合+下只减伤族（983，soul_lib/k188_death_break_bequest.cpp）
//（2026-10-07，worktree ../agents/k188，看板段 3200-3209 → T3200-T3203）
//
// 断言口径（杀局：对手速 300 先手拍死 hp=1 的持印者 → FIRST_MOVER_DEATH 桶触发）：
//   - T3200 断回合（对照对局对）：给对手种一个可断回合窗（ContinuousEffect 直种，
//     moves register_round_effect 同构）→ 死亡节点 break → round_effect_valid_id[1]
//     递增、窗增伤消失；对照局（无印）不递增。
//     ⚠️ 实测记档：seal_skill 的次数型/回合型封属**不在** break_round_effects 的
//     可断面内（has_round_effects/has_round_type 均不可见，返回 NO_EFFECTS）——
//     断回合观测载体必须用 ContinuousEffect 回合窗。
//   - T3201 承担次数=2（本体行 [2,50,1]，对照对局对同 seed）：接班精灵 R2/R3 受击减半
//     （loss_m == loss_c - loss_c/2 整数式精确），R4 次数耗尽不再减（与对照相等）。
//     断回合/减伤回调均零掷点 → 两局 rand 流对齐。
//   - T3202 伙伴行 [3,50,1]：R2/R3/R4 三次减半，R5 不减（行参驱动次数）。
//   - T3203（BOSS无效）：boss 挑战局死亡节点不执行 → valid_id 不递增（无断回合）。
#include <array>
#include <iostream>

#include <effects/continuousEffect.h>
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
ElfPet mark_pet(EffectArgs mark_args, int speed, int hp) {
    SoulMark sm(MarkId, "共承天地魂印", "", std::move(mark_args));
    sm.monster_id = 3385;   // 行级口径字段（boss 门读 monster_id 的家族习惯，本单不判）
    ElfPet p = make_pet(3385, "步练师", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, hp, /*soul_mark=*/std::move(sm));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_successor(int speed) {
    ElfPet p = make_pet(2, "接班", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(3, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

std::array<ElfPet, 6> own_party(EffectArgs mark_args, bool with_mark) {
    // 持印者速 100（后手位、hp=1 被先手拍死）；接班者速 100 占槽 1。
    ElfPet holder = with_mark
        ? mark_pet<983>(std::move(mark_args), /*speed=*/100, /*hp=*/1)
        : plain_successor(/*speed=*/100);
    if (!with_mark) {
        holder.hp = 1;   // 对照局同杀局
    }
    return {holder, plain_successor(100), plain_successor(100),
            plain_successor(100), plain_successor(100), plain_successor(100)};
}

// 可断回合窗（moves register_round_effect 同构直种）：对手每 ROUND_START 攻击增伤 +100
// （AMP 整数式 → 存活期伤害恰 2×），被断后窗口作废、增伤消失。
EffectResult opp_amp_window_fn(BattleContext* c, const EffectArgs&) {
    if (c) {
        c->ws.damage_add_pct[1] += 100.0f;
    }
    return EffectResult::kOk;
}

void plant_amp_window(ScenarioBattle& b) {
    BattleContext& x = b.ctx();
    Effect e(983002, 0, 1, 5, EffectArgs(std::vector<int>{}), &opp_amp_window_fn);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, 1, 5,
                                                 x.roundCount);
    x.register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3200 断回合：死亡 → 对手可断回合票被打断（valid_id 递增）══
    {
        int marked_bump = -1;
        int plain_bump = -1;
        for (int side = 0; side < 2; ++side) {
            ScenarioBattle b(own_party(EffectArgs(std::vector<int>{2, 50, 1}),
                                       /*with_mark=*/side == 0),
                             make_party(plain_foe(300)));
            BattleContext& x = b.ctx();
            sbs_srand(7);
            b.start();
            // 对手挂"仅封属性技"次数票（可断回合类；不挡其攻击 → 照常杀人）
            plant_amp_window(b);   // 对手名下种可断回合窗（死亡断回合应将其打断）
            const int baseline = x.round_effect_valid_id[1];
            both_attack(b);   // R1：对手先手拍死 hp=1 持印者
            CHECK(x.getPet(0).hp <= 0, "T3200 前置：持印者被击倒");
            const int bump = x.round_effect_valid_id[1] - baseline;
            if (side == 0) {
                marked_bump = bump;
            } else {
                plain_bump = bump;
            }
        }
        CHECK(marked_bump >= 1, "T3200 983：死亡断回合 → 对手回合窗作废（valid_id 递增）");
        CHECK_EQ(plain_bump, 0, "T3200 对照：无印 → 不断回合");
        std::cout << "  [T3200] 断回合 通过 (bump=" << marked_bump << ")\n";
    }

    // ══ T3201/T3202 承担减伤：本体 2 次 / 伙伴行 3 次，耗尽即止（对照对局对）══
    {
        const int counts_opt[2] = {2, 3};
        for (int row = 0; row < 2; ++row) {
            int loss_marked[6] = {};
            int loss_plain[6] = {};
            for (int side = 0; side < 2; ++side) {
                ScenarioBattle b(own_party(
                                     EffectArgs(std::vector<int>{counts_opt[row], 50, 1}),
                                     /*with_mark=*/side == 0),
                                 make_party(plain_foe(300)));
                sbs_srand(7);
                b.start();
                both_attack(b);   // R1：杀局
                CHECK(b.ctx().getPet(0).hp <= 0, "T3201 前置：持印者被击倒");
                b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();   // R2 开局：接班者登场
                CHECK(b.ctx().getPet(0).hp > 0, "T3201 前置：接班者已登场");
                for (int r = 0; r < 5; ++r) {   // r0=换人回合（对手已 NONE→0 伤）；r1起全攻
                    const int before = b.ctx().getPet(0).hp;
                    both_attack(b);
                    (side == 0 ? loss_marked : loss_plain)[r] = before - b.ctx().getPet(0).hp;
                }
            }
            for (int r = 0; r < 5; ++r) {
                // r0=换人回合双方 0 伤；承担期 = r1..r_counts；之后恢复全伤
                const bool expect_half = r >= 1 && r <= counts_opt[row];
                if (expect_half) {
                    CHECK_EQ(loss_marked[r], loss_plain[r] - loss_plain[r] / 2,
                             (row == 0 ? "T3201 983：承担期内受击减半（整数式）"
                                       : "T3202 983：伙伴行承担期内受击减半"));
                } else {
                    CHECK_EQ(loss_marked[r], loss_plain[r],
                             (row == 0 ? "T3201 983：2 次耗尽 → 不再减"
                                       : "T3202 983：3 次耗尽 → 不再减"));
                }
            }
            std::cout << "  [T320" << (row + 1) << "] 承担" << counts_opt[row]
                      << "次减半 通过 (r2: " << loss_plain[0] << "→" << loss_marked[0]
                      << ")\n";
        }
    }

    // ══ T3203（BOSS无效）：boss 挑战局不断回合 ══
    {
        ScenarioBattle b(own_party(EffectArgs(std::vector<int>{2, 50, 1}),
                                   /*with_mark=*/true),
                         make_party(plain_foe(300)));
        BattleContext& x = b.ctx();
        x.is_boss_challenge = true;
        sbs_srand(7);
        b.start();
        plant_amp_window(b);
        const int baseline = x.round_effect_valid_id[1];
        both_attack(b);
        CHECK_EQ(x.round_effect_valid_id[1] - baseline, 0,
                 "T3203 983：boss 挑战局（BOSS无效）→ 死亡节点不执行");
        std::cout << "  [T3203] BOSS无效 通过\n";
    }

    std::cout << "scenario_3200 PASS (K188 死亡消回合+下只减伤族)" << std::endl;
    return 0;
}
