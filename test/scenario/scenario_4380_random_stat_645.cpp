#include <utils/sbs_rand.h>
// 场景 4380 —— K282 回合随机属性+1族（645，soul_lib/k282_random_stat.cpp）
//（2026-10-21，worktree ../agents/k282；号段 4380-4389 → T4380-T4381 两块）
//
// 断言口径（645「每回合随机2项属性+1」）：
//   - T4380 第 1 回合：自身**恰有 2 槽**为 +1（其余为 0）——随机槽不可预测，
//     断言"槽数"而非"具体槽"（K 系惯例：随机类用计数断言）。
//   - T4381 第 2 回合：继续提升（累计槽位数 ≥ 2；若随机到已提升的槽则维持 +1，
//     等级和进一步增加）——"每回合"语义。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <algorithm>
#include <array>
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

ElfPet mark_pet(std::vector<int> args = {2, 1}) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(645, "K282 魂印", "",
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

// 统计某方等级为正（> 0）的槽数。
int raised_slot_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// 某方等级和。
int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

using Levels = std::array<int, BattleContext::kAbilityLevelSlotCount>;

Levels snapshot(const BattleContext& ctx) {
    Levels out{};
    for (int i = 0; i < static_cast<int>(out.size()); ++i) {
        out[i] = ctx.ability_levels[0][i];
    }
    return out;
}

// 非满级起测：逐槽锁每次恰提升 count 个不同槽，不能同槽叠 delta。
int check_distinct_delta(const Levels& before, const Levels& after, int count, int delta) {
    int changed = 0;
    int mask = 0;
    for (int i = 0; i < static_cast<int>(before.size()); ++i) {
        const int diff = after[i] - before[i];
        CHECK(diff == 0 || diff == delta, "645：单槽每次至多提升一份幅度");
        if (diff != 0) {
            ++changed;
            mask |= 1 << i;
        }
    }
    CHECK_EQ(changed, delta == 0 ? 0 : count, "645：每次提升的不同槽数恰等于有界 count");
    return mask;
}

Levels probe_round(std::vector<int> args, unsigned seed, const Levels& before) {
    auto party = make_party(plain_pet());
    party[0] = mark_pet({0, 0});   // 场下同号行不能抢实际宿主参数
    party[1] = mark_pet(std::move(args));
    ScenarioBattle b(party, make_party(plain_pet()));
    b.ctx().on_stage[0] = 1;
    b.start();
    for (int i = 0; i < static_cast<int>(before.size()); ++i) {
        b.ctx().ability_levels[0][i] = before[i];
    }
    sbs_srand(seed);
    one_round(b);
    return snapshot(b.ctx());
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4380 第 1 回合：恰有 2 槽 +1 ═══════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        one_round(b);
        CHECK_EQ(raised_slot_count(ctx, 0), 2,
                 "T4380 645：每回合随机 2 项属性 +1（恰 2 槽被提升）");
        CHECK_EQ(level_sum(ctx, 0), 2,
                 "T4380 645：提升总量 = 2 项 × 1 级 = 2");
        std::cout << "  [T4380] 随机提升 通过（slots="
                  << raised_slot_count(ctx, 0) << " sum=" << level_sum(ctx, 0) << "）\n";
    }

    // ══ T4381 第 2 回合：继续提升 ═══════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        const Levels before = snapshot(ctx);
        one_round(b);
        const Levels first = snapshot(ctx);
        const int sum1 = level_sum(ctx, 0);
        check_distinct_delta(before, first, 2, 1);
        one_round(b);
        const int sum2 = level_sum(ctx, 0);
        check_distinct_delta(first, snapshot(ctx), 2, 1);
        CHECK_EQ(sum1, 2, "T4381 645 前置：第 1 回合提升 2 级");
        CHECK_EQ(sum2, 4, "T4381 645：第 2 回合仍恰提升 2 级");
        std::cout << "  [T4381] 每回合持续 通过（sum=" << sum1 << "→" << sum2 << "）\n";
    }

    // 新采样消费序列不与历史 golden 绑定：多 seed 每一局都检查去重和原序行参。
    struct StatCase {
        std::vector<int> args;
        int count;
        int delta;
    };
    const std::vector<StatCase> cases{
        {{2, 1}, 2, 1}, {{3, 2}, 3, 2}, {{0, 1}, 0, 1},
        {{6, 1}, 6, 1}, {{99, 2}, 6, 2}, {{2, 0}, 2, 0},
        {{}, 2, 1}, {{3}, 3, 1}, {{-1, 2}, 0, 2},
    };
    int seen_mask = 0;
    for (unsigned n = 0; n < 24; ++n) {
        const unsigned seed = 20261106u + n * 7919u;
        for (const auto& c : cases) {
            const Levels before{};
            const Levels after = probe_round(c.args, seed, before);
            const int mask = check_distinct_delta(before, after, c.count, c.delta);
            if (c.count == 2 && c.delta == 1) {
                seen_mask |= mask;
            }
        }

        // 同 seed 的非满级局给出真实被选集合。把其中一个槽置 +6 后必须仍占名额，
        // 其余逐槽结果不变；预筛满级槽/补抽会多升一槽。无需生产代码审计槽或旧 golden。
        const Levels zero{};
        const Levels baseline = probe_round({2, 1}, seed, zero);
        int capped_slot = 0;
        while (capped_slot < static_cast<int>(baseline.size()) && baseline[capped_slot] == 0) {
            ++capped_slot;
        }
        REQUIRE(capped_slot < static_cast<int>(baseline.size()));
        Levels capped_before{};
        capped_before[capped_slot] = 6;
        const Levels capped_after = probe_round({2, 1}, seed, capped_before);
        for (int slot = 0; slot < static_cast<int>(baseline.size()); ++slot) {
            CHECK_EQ(capped_after[slot], std::min(6, capped_before[slot] + baseline[slot]),
                     "645：+6 槽仍参与同一抽样、不补抽，等级只由原语钳顶");
        }
    }
    CHECK_EQ(seen_mask, (1 << BattleContext::kAbilityLevelSlotCount) - 1,
             "645：多 seed 覆盖全部六槽（含命中），不是固定两槽");

    std::cout << "scenario_4380 PASS (K282 回合随机属性+1族)" << std::endl;
    return 0;
}
