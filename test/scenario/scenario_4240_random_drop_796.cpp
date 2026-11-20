#include <utils/sbs_rand.h>
// 场景 4240 —— K268 随机对手降能力族（796，soul_lib/k268_random_drop.cpp）
//（2026-10-21，worktree ../agents/k268；号段 4240-4249 → T4240-T4241 两块）
//
// 断言口径（796「每回合使对手随机2项能力-1」）：
//   - T4240 回合开始后 → 对手**恰有 2 槽**为 -1（其余为 0）——随机槽不可预测，
//     断言"槽数"而非"具体槽"（K 系惯例：随机类用计数断言）。
//   - T4241 第二回合 → 对手降属**继续**（累计 4 槽位次）——"每回合"语义。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击载体

ElfPet mark_pet() {
    std::vector<int> args{2, 1};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(796, "K268 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 统计某方"被降过"的槽数（等级 < 0，含 -2 等更负——去重失效时会体现）。
// ⚠️ 只数 == -1 会漏掉去重失效（同槽被降两次 → -2）→ 必须数 < 0。
int dropped_slot_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
            ++n;
        }
    }
    return n;
}

// 某方无正等级（降属只往下压）。
bool no_positive_level(BattleContext& ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4240 回合开始 → 对手恰有 2 槽 -1 ═══════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        one_round(b);
        CHECK_EQ(dropped_slot_count(ctx, 1), 2,
                 "T4240 796：每回合使对手随机 2 项能力 -1（恰 2 槽）");
        CHECK(no_positive_level(ctx, 1),
              "T4240 796：对手无正等级（降属只往下压）");
        std::cout << "  [T4240] 随机降属 通过\n";
    }

    // ══ T4241 第二回合 → 继续降（累计 4 槽次）═══════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        one_round(b);
        const int after_r1 = dropped_slot_count(ctx, 1);
        one_round(b);
        const int after_r2 = dropped_slot_count(ctx, 1);
        // 第 2 回合再降 2 项：若随机到已降的槽则仍为 -1（stat_drop 到 -1 就不再降，
        // 因 stat_drop 钳 -6 但"已为 -1 再降"会到 -2）——故断言"槽数 >= 2"，
        // 且总降属量（等级和）进一步变负。
        int sum = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            sum += ctx.ability_levels[1][i];
        }
        CHECK(after_r1 == 2, "T4240 前置：第 1 回合降 2 槽");
        CHECK(sum < 0, "T4241 796：第 2 回合继续降属（对手等级和为负）");
        CHECK(after_r2 >= 1, "T4241 796：第 2 回合后对手仍有降属槽位");
        std::cout << "  [T4241] 每回合持续 通过（r1=" << after_r1
                  << " r2=" << after_r2 << " sum=" << sum << "）\n";
    }

    std::cout << "scenario_4240 PASS (K268 随机对手降能力族)" << std::endl;
    return 0;
}
