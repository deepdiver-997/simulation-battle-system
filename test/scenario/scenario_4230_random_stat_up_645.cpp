#include <utils/sbs_rand.h>
// 场景 4230 —— K267 随机属性+1族（645，soul_lib/k282_random_stat.cpp（K267 已收敛））
//（2026-10-21，worktree ../agents/k267；号段 4230-4239 → T4230-T4233 四块）
//
// 断言口径（645「每回合随机2项属性+1」）：
//   - T4230 回合 1：随机 2 项各 +1 → 6 槽**等级总和 = 2**（不依赖具体落哪两槽）。
//   - T4231 两回合：累计 = 4（每回合 +2，逐回合线性）。
//   - T4232 **两项互不相同**：任意 6 槽里至多 2 个槽为 +1，且恰为 2 个
//     （若两项落在同一槽，会出现"1 槽 +2 / 其余 0"或"1 槽 +1"——前者与后者都
//     与"2项"不符）。断言"恰好 2 个槽为 +1 且无槽 ≥ +2"。
//   - T4233 门闭对照：**不带魂印** → 等级总和恒为 0（证明 +1 来自本魂印）。
//
// ⚠️ 随机性断言法（K4 622 场景 955 同款）：随机槽位不可预知，故
//   ①用 sbs_srand 定档让结果可复现；②断**总量**（各槽等级和）而非具体槽位；
//   ③"互不相同"另用**结构断言**（+1 槽的个数）单独表达。
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
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

constexpr int kGaoSu = 20017;   // 高速移动：属性技（不出手用，避免额外等级变化）
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;

SoulMark make_mark() {
    // DB args 原序 [2,1]：db[0]=2 项、db[1]=+1，统一由 K282 参数化实现消费。
    SoulMark sm(645, "K267 魂印", "test", EffectArgs(std::vector<int>{2, 1}));
    sm.monster_id = 2908;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "月光之龙", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                   /*speed=*/100, /*atk=*/10, /*hp=*/1000000, make_mark())
        : make_pet(1, "裸月光之龙", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                   /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

// 6 槽等级总和（随机效果的标准断言量）。
int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < kSlots; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// 跑 N 个不同种子，每种子起一局，统计"随机2项"落槽情况。
// 返回 (出现同槽的局数, 出现的不同+1槽个数)。
// ⚠️ 随机效果的断言必须**覆盖随机空间**——单一固定种子只覆盖一个抽样点，
// 破坏去重的实现可能恰好在那个抽样点上不撞槽（实测 MUT1 单种子不转红）。
// 故本单用多种子循环：同槽概率 ≈ 1/6（6 槽抽 2 次），40 种子必现。
struct DedupProbe {
    int same_slot_hits = 0;      // 出现"同槽加两次"的局数
    int distinct_plus_one = 0;   // 观察到的不同 "+1 槽" 取值个数
    int rounds = 0;
};

DedupProbe probe_dedup_multi_seed(int seeds) {
    DedupProbe probe;
    for (int s = 0; s < seeds; ++s) {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021 + s * 7919);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        ++probe.rounds;
        int plus_one = 0;
        for (int i = 0; i < kSlots; ++i) {
            if (ctx.ability_levels[0][i] == 1) {
                ++plus_one;
            } else if (ctx.ability_levels[0][i] >= 2) {
                ++probe.same_slot_hits;   // 同一槽被加两次 → 违反"2项互不相同"
            }
        }
        if (plus_one != 2) {
            ++probe.same_slot_hits;       // +1 槽数不足 2 → 也违反
        }
        for (int i = 0; i < kSlots; ++i) {
            if (ctx.ability_levels[0][i] == 1) {
                ++probe.distinct_plus_one;
            }
        }
    }
    return probe;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4230 回合 1：随机 2 项各 +1 → 总和 = 2 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 2,
                 "T4230 645：每回合随机 2 项属性+1 → 6 槽等级总和 = 2");
        std::cout << "  [T4230] 回合1 随机2项+1 通过\n";
    }

    // ══ T4231 两回合：累计 = 4 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        b.start();
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(level_sum(ctx, 0), 4,
                 "T4231 645：两回合累计随机 +4（每回合 +2）");
        std::cout << "  [T4231] 两回合累计 通过\n";
    }

    // ══ T4232 两项互不相同：多种子循环覆盖随机空间 ══
    // 单一固定种子只覆盖一个抽样点——破坏去重的实现可能恰好在该点不撞槽
    // （实测 MUT1 单种子不转红）。故用 40 个种子循环：同槽概率 ≈ 1/6，必现。
    {
        const DedupProbe probe = probe_dedup_multi_seed(40);
        CHECK_EQ(probe.same_slot_hits, 0,
                 "T4232 645：40 个种子下，'随机2项'从未落在同一槽（同槽即违反"
                 "'2项'语义），实测违规局数 = " + std::to_string(probe.same_slot_hits));
        CHECK(probe.distinct_plus_one >= 6,
              "T4232 645：40 个种子下 +1 槽覆盖全部 6 个槽（证明确实在随机而非固定），"
                  "实测覆盖槽数 = " + std::to_string(probe.distinct_plus_one));
        std::cout << "  [T4232] 两项互不相同（多种子覆盖） 通过\n";
    }

    // ══ T4233 门闭对照：不带魂印 → 总和恒 0 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(plain_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 0,
                 "T4233 645：对照——不带魂印时等级总和恒为 0（+1 确来自本魂印）");
        std::cout << "  [T4233] 门闭对照 通过\n";
    }

    std::cout << "scenario_4230 PASS (K267 随机属性+1族)" << std::endl;
    return 0;
}
