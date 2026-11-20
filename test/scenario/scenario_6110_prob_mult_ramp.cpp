#include <utils/sbs_rand.h>
// 场景 6110 —— J278 概率倍伤递增族（1242，moves_lib/j_prob_mult_ramp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6110-6119 → T6110.a-c）
//
// 断言口径（坑集 §0：实验组只在被测变量上变化；观测值不落 clamp 区间）：
//   - 载体 天·混沌之瞳 32308 [191,1242]|[3,50,2,25,3]（1242=[50,2,25,3]；191=3回合
//     免疫反弹异常，对伤害无影响）。对手每回合 NONE，掉血只来自本技能（红伤）。
//   - 审计槽精确断言概率状态机：首次 50%/×2；未触发 k 次后 min(50+k×25,100)/×3；
//     命中后 k 清零（991242000/100/200 = 本次概率/是否命中/倍率）。
//   - 伤害绝对量带（速查 §五：atk300/def100 的 raw 锚，power150 → base 约 [323,380]）：
//       未触发 base∈[300,420]；×2 命中∈[600,800]；×3 命中∈[900,1250]——
//     相邻档间距远大于 ±5% 浮动，用绝对量而非"与对照局相等"（坑集 §2）。
//   - 多 seed（0/1/8/9）覆盖：首回合未触发、首次命中、递增后 ×3、概率封顶 100 四分支；
//     seed=9 的第 3 回合由 50→75→100 的递增吞掉随机性（100% 必中），锁 cap 口径。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 32308;   // 天·混沌之瞳：1242=[50,2,25,3]
constexpr int kFiller  = 10004;   // 拍打
constexpr int kMaxHp   = 99999;
constexpr int kRounds  = 5;       // 32308 PP=5：5 回合内不会因 PP 耗尽跳过出手

constexpr int kChanceAuditBase = 991242000;
constexpr int kProcAuditBase   = 991242100;
constexpr int kMultAuditBase   = 991242200;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

struct RoundTrace {
    int loss = 0;
    int chance = -2;
    int proc = -2;
    int mult = -2;
};

std::vector<RoundTrace> run_battle(unsigned seed) {
    ElfPet self = make_pet(1, "己方", {kCarrier, kFiller, kFiller, kFiller, kFiller},
                           /*speed=*/300, /*atk=*/300, kMaxHp);
    for (Skills& s : self.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    ElfPet foe = make_pet(2, "对手", {kFiller, kFiller, kFiller, kFiller, kFiller},
                          /*speed=*/100, /*atk=*/1, kMaxHp);
    sbs_srand(seed);
    ScenarioBattle b(make_party(self), make_party(foe));
    b.start();
    BattleContext& ctx = b.ctx();
    std::vector<RoundTrace> out;
    for (int r = 0; r < kRounds; ++r) {
        const int before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        RoundTrace t;
        t.loss = before - ctx.getPet(1).hp;
        t.chance = audit_value(ctx, kChanceAuditBase + 0);
        t.proc = audit_value(ctx, kProcAuditBase + 0);
        t.mult = audit_value(ctx, kMultAuditBase + 0);
        out.push_back(t);
    }
    return out;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const int seeds[] = {0, 1, 8, 9};
    bool saw_fail = false, saw_proc2 = false, saw_proc3 = false, saw_cap100 = false;

    for (int seed : seeds) {
        const std::vector<RoundTrace> tr = run_battle(static_cast<unsigned>(seed));
        int streak = 0;   // 测试侧独立重算状态机（不读实现的 streak 槽）
        for (int r = 0; r < kRounds; ++r) {
            const RoundTrace& t = tr[static_cast<std::size_t>(r)];
            const int expect_chance = std::min(50 + streak * 25, 100);
            const int expect_mult = streak > 0 ? 3 : 2;
            const std::string tag = "seed" + std::to_string(seed) + ".r" + std::to_string(r);

            CHECK_EQ(t.chance, expect_chance, "T6110.a " + tag + " 申报概率状态机");
            CHECK_EQ(t.mult, expect_mult, "T6110.a " + tag + " 本次倍率状态机");
            CHECK(t.proc == 0 || t.proc == 1, "T6110.a " + tag + " 审计命中标记 ∈ {0,1}");

            if (t.proc == 0) {
                saw_fail = true;
                CHECK(t.loss >= 300 && t.loss <= 420,
                      "T6110.b " + tag + " 未触发 → base 档 [" + std::to_string(t.loss) + "]");
                ++streak;
            } else {
                CHECK(t.loss >= 600, "T6110.b " + tag + " 命中 → 掉血高于 base（"
                                         + std::to_string(t.loss) + "）");
                if (t.mult == 2) {
                    saw_proc2 = true;
                    CHECK(t.loss <= 800, "T6110.b " + tag + " ×2 档上界");
                } else {
                    saw_proc3 = true;
                    CHECK(t.loss >= 900 && t.loss <= 1250,
                          "T6110.b " + tag + " ×3 档 [" + std::to_string(t.loss) + "]");
                }
                streak = 0;
            }
            if (t.chance == 100) {
                saw_cap100 = true;
            }
        }
    }

    // 分支覆盖（自校准：不依赖固定 seed 的抽签结果）。
    CHECK(saw_fail, "T6110.c 至少覆盖一次未触发（base 档）");
    CHECK(saw_proc2, "T6110.c 至少覆盖一次 ×2 命中");
    CHECK(saw_proc3, "T6110.c 至少覆盖一次递增后 ×3 命中");
    CHECK(saw_cap100, "T6110.c 至少覆盖一次概率递增封顶 100%");

    std::cout << "scenario_6110 PASS (J278 概率倍伤递增族)" << std::endl;
    return 0;
}
