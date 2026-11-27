// 场景 780 —— G8 数据驱动老效果 21×48（moves_lib/e_reflect_old21.cpp）
//（2026-09-29，worktree ../agents/g8）
//
// 机制：老"反弹护体"属性技挂盾族（effect_info 无行的数据驱动效果，语义考古
// DB 切片 + 4399 图鉴双源定案）：args [a,b,c] = 作用 a~b 回合（含当回合，roll 一次），
// 期间每次受直接攻击伤害（红伤）反弹 1/c 给攻击方。EVENT_TAKE_DAMAGE watcher
//（462/580 e_reflect.cpp 同通道）。ctx->roundCount 从 0 起算：挂盾回合=0，持续 d
// ⇒ 覆盖 roundCount 0..d-1（挂盾当回合起 d 个"场景回合"），第 d+1 个场景回合起哑火。
//
// 断言组（反弹量 = taken/c 精确整数等式；伤害浮动只进 taken 本身）：
//   T1 寄宿 20450 [1,1,1]：挂盾当回合被攻击 → 对手恰掉 taken×1/1；R2 → 0（1 回合窗）；
//   T2 坚固反击 21007 [2,2,2]：R1/R2 反弹 = taken×1/2，R3 → 0（2 回合窗边界）；
//   T3 灵光反冲 20100 [2,3,3]：R1 反弹 = taken×1/3；30 局统计 R3 是否反弹 →
//      时长 2 与 3 两种 roll 都出现（范围 roll 覆盖）；
//   T4 组合载体 音波护盾 22149 [4,4,21]（切片 [1,100,1][3,100,1][2,2,2]）：
//      双防+1 与 反弹（R1 = taken/2、R3 → 0）同技并存——多效果切片路径；
//   T5 对照：无盾挨打 → 对手不掉血（反弹确由 21 watcher 引起）。
#include <iostream>
#include <utility>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJisux = 20450;      // 寄宿 [1,1,1]：1回合 反弹1/1
constexpr int kJianggu = 21007;    // 坚固反击 [2,2,2]：2回合 反弹1/2
constexpr int kLingguang = 20100;  // 灵光反冲 [2,3,3]：2~3回合 反弹1/3
constexpr int kYinbo = 22149;      // 音波护盾 [4,4,21]：双防+1 + 2回合 反弹1/2
constexpr int kZhuangJi = 10001;   // 撞击（对手攻击技）
constexpr int kPassive = 10938;    // 本能一击：威力 0（挂盾方填充技）

// 挂盾方先手（speed 高）——"作用 N 回合"含使用回合：先手挂盾、后手同回合挨打，
// 当回合就能验反弹；填充技全 NONE，零污染对手血量断言。
ElfPet shielder(int skill) {
    ElfPet p = make_pet(1, "挂盾方", {skill, kPassive, kPassive, kPassive, kPassive},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet striker(int speed = 100) {
    ElfPet p = make_pet(2, "攻击方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, /*atk=*/100, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 挂盾回合：挂盾方用技能（先手）+ 对手攻击（后手），一 drive 打完整回合。
// 返回 <挂盾方掉血, 对手掉血>。
std::pair<int, int> arm_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int self_before = ctx.getPet(0).hp;
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return {self_before - ctx.getPet(0).hp, foe_before - ctx.getPet(1).hp};
}

// 后续回合：挂盾方 NONE（零污染）、对手攻击。返回 <挂盾方掉血, 对手掉血>。
std::pair<int, int> round_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int self_before = ctx.getPet(0).hp;
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return {self_before - ctx.getPet(0).hp, foe_before - ctx.getPet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 寄宿 [1,1,1]：当回合反弹 1/1，次回合窗口已闭
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kJisux)), make_party(striker()));
        b.start();
        auto [taken1, foe_drop1] = arm_round(b);   // R1：先手挂盾，后手挨打
        CHECK(taken1 > 0, "T1 前置：挂盾当回合确实挨到了红伤");
        CHECK_EQ(foe_drop1, taken1, "T1 核心：1/1 反弹——对手恰掉等量体力");
        auto [taken2, foe_drop2] = round_strike(b);  // R2：1 回合窗已闭
        CHECK(taken2 > 0, "T1 前置：R2 仍挨到红伤");
        CHECK_EQ(foe_drop2, 0, "T1 窗口：作用 1 回合——R2 不再反弹");
        std::cout << "  [T1] 寄宿 [1,1,1] 通过 (taken=" << taken1 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 坚固反击 [2,2,2]：R1/R2 反弹 1/2，R3 窗口闭
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kJianggu)), make_party(striker()));
        b.start();
        auto [taken1, drop1] = arm_round(b);
        CHECK(taken1 > 0, "T2 前置 R1 挨打");
        CHECK_EQ(drop1, taken1 / 2, "T2 核心 R1：反弹 = 受伤 1/2");
        auto [taken2, drop2] = round_strike(b);
        CHECK(taken2 > 0, "T2 前置 R2 挨打");
        CHECK_EQ(drop2, taken2 / 2, "T2 核心 R2：第 2 回合仍在窗口内");
        auto [taken3, drop3] = round_strike(b);
        CHECK(taken3 > 0, "T2 前置 R3 挨打");
        CHECK_EQ(drop3, 0, "T2 窗口：作用 2 回合——R3 起哑火");
        std::cout << "  [T2] 坚固反击 [2,2,2] 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 灵光反冲 [2,3,3]：R1 反弹 1/3；30 局统计时长 roll 2/3 都出现
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kLingguang)), make_party(striker()));
        b.start();
        auto [taken1, drop1] = arm_round(b);
        CHECK(taken1 > 0, "T3 前置 R1 挨打");
        CHECK_EQ(drop1, taken1 / 3, "T3 核心 R1：反弹 = 受伤 1/3");
        // 统计：R3 反弹 ⇔ roll 出 3 回合；R3 不反弹 ⇔ roll 出 2。范围 roll 两者都该出现。
        int seen2 = 0, seen3 = 0;
        SbsRng trial_seeds(20261004);
        for (int i = 0; i < 30; ++i) {
            ScenarioBattle bi(make_party(shielder(kLingguang)), make_party(striker()));
            bi.ctx().seed_rng(trial_seeds.next());
            bi.start();
            (void)arm_round(bi);              // R1：挂盾 + 挨打（推进到窗口内）
            (void)round_strike(bi);           // R2：必在窗内（仅推进回合）
            auto [t3, d3] = round_strike(bi);  // R3：反弹 ⇔ 时长 3
            if (d3 > 0) {
                ++seen3;
            } else {
                ++seen2;
            }
        }
        CHECK(seen2 > 0 && seen3 > 0, "T3 范围 roll：时长 2 与 3 均应出现（统计 30 局）");
        std::cout << "  [T3] 灵光反冲 [2,3,3] 通过（时长 roll：2×" << seen2
                  << " / 3×" << seen3 << "）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 组合载体 音波护盾 [4,4,21]：双防+1 与反弹并存（切片路径）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kYinbo)), make_party(striker()));
        b.start();
        auto [taken1, drop1] = arm_round(b);   // R1：挂盾（含双防+1）+ 挨打
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][1], 1, "T4 组合：防御等级 +1（4 号切片[1,100,1]）");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "T4 组合：特防等级 +1（4 号切片[3,100,1]）");
        CHECK(taken1 > 0, "T4 前置 R1 挨打");
        CHECK_EQ(drop1, taken1 / 2, "T4 组合：21 切片[2,2,2] 反弹 1/2 生效");
        (void)round_strike(b);                 // R2 仍在窗内
        auto [taken3, drop3] = round_strike(b);  // R3 窗闭
        CHECK(taken3 > 0, "T4 前置 R3 挨打");
        CHECK_EQ(drop3, 0, "T4 窗口：组合切片同为 2 回合——R3 哑火");
        std::cout << "  [T4] 音波护盾 [4,4,21] 组合切片 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 对照：无盾挨打 → 对手不掉血
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kZhuangJi)), make_party(striker()));
        b.start();
        auto [taken, foe_drop] = round_strike(b);
        CHECK(taken > 0, "T5 前置：确实挨到了红伤");
        CHECK_EQ(foe_drop, 0, "T5 对照：无盾不反弹（回敬确由 21 watcher 引起）");
        std::cout << "  [T5] 无盾对照 通过\n";
    }

    std::cout << "scenario_780 PASS (G8 老效果 21 反弹护体族)" << std::endl;
    return 0;
}
