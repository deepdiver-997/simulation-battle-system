// 场景 1600 —— J73 随机削血/降速命中族（140/409，moves_lib/j_randdrain_slowhit.cpp）
//（2026-10-03，worktree ../agents/j73；号段 1600-1609 → T1600-T1601 两块）
//
// 断言口径：
//   - 140 随机分母削血（21342 惩戒之光 [8,2] 属性技，无红伤干扰）：粉 FINAL 探针
//     每回合恰 1 笔、且金额 ∈ {cur/k : k=2..8}（整数集合逐一核对，随机分母的
//     确定性等价断言）；6 回合聚合。
//   - 409 速/命中双槽窗口（17336 背影突击 [3,-1]）：R1 内联 → 槽4/槽5 == -1；
//     R2/R3 ROUND_START tick → -2/-3；R4 窗外 → 保持 -3（等级持久）。
//   atk=1 消红噪声；对手 NONE。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（填充刀）
constexpr int kChengJie = 21342;   // 惩戒之光 [140]|[8,2] 属性技（随机削血）
constexpr int kBeiYing  = 17336;   // 背影突击 [409]|[3,-1] 物85（降速命中窗）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "削血族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void install_pink_probe(BattleContext& ctx, std::vector<int>& pinks) {
    ctx.register_pink_damage_effect(
        PinkDamagePhase::FINAL, 1, [&pinks](BattleContext* c, int) {
            const PinkDamageResolved& p = c->resolvedPink;
            if (p.target == 1 && p.final > 0) {
                pinks.push_back(p.final);
            }
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1600 140 随机分母削血（21342 [8,2]）══
    {
        ScenarioBattle b(
            make_party(self_pet({kChengJie, kChengJie, kChengJie, kChengJie, kChengJie})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        std::vector<int> pinks;
        install_pink_probe(ctx, pinks);
        int cur = 99998;
        for (int r = 0; r < 3; ++r) {   // 惩戒之光 max_pp=3，恰 3 轮
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(pinks.size()), 1,
                     "T1600 140 第 " + std::to_string(r + 1) + " 轮恰 1 笔粉伤");
            const int amount = pinks.back();
            bool in_set = false;
            for (int k = 2; k <= 8; ++k) {
                if (amount == cur / k) {
                    in_set = true;
                }
            }
            CHECK(in_set, "T1600 140 第 " + std::to_string(r + 1) +
                              " 轮金额 ∈ {cur/2..cur/8}（amount=" + std::to_string(amount) +
                              "，cur=" + std::to_string(cur) + "）");
            cur -= amount;
            pinks.clear();
        }
        std::cout << "  [T1600] 140 随机分母削血 通过\n";
    }

    // ══ T1601 409 速/命中双槽窗口（17336 [3,-1]）══
    {
        ScenarioBattle b(
            make_party(self_pet({kBeiYing, kPaiDa, kPaiDa, kPaiDa,
                                 kPaiDa})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 1; r <= 4; ++r) {
            // 只在 R1 用一次 409 载体（重复使用会内联再施加、窗口重叠叠层——行为正确），
            // R2-R4 用拍打，单窗口演化：R1 -1 / R2 -2 / R3 -3 / R4 保持 -3
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r == 1 ? 0 : 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int expect = (r <= 3) ? -r : -3;   // 窗 3 回合，R4 窗外保持
            CHECK_EQ(ctx.ability_levels[1][4], expect,
                     "T1601 409 R" + std::to_string(r) + " 速槽4 == " +
                         std::to_string(expect));
            CHECK_EQ(ctx.ability_levels[1][5], expect,
                     "T1601 409 R" + std::to_string(r) + " 命中槽5 == " +
                         std::to_string(expect));
        }
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T1601 409 攻槽不受影响");
        std::cout << "  [T1601] 409 降速命中窗 通过\n";
    }

    std::cout << "scenario_1600 PASS (J73 随机削血/降速命中族)" << std::endl;
    return 0;
}
