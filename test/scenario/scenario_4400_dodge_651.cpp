#include <utils/sbs_rand.h>
// 场景 4400 —— K284 概率闪避族（651，soul_lib/k284_dodge.cpp）
//（2026-10-21，worktree ../agents/k284；号段 4400-4409 → T4400-T4403 四块）
//
// 断言口径（651「50%概率闪避对手的攻击（boss无效）」）：
//   - T4400 **闪避率生效**：单局 1 次攻击，用**探针标定的命中种子** → 印主不掉血
//     （闪避发生）；对照同一场景的无印局 → 掉血。ws.dodge_rate 把命中率改成
//     `accuracy - dodge*100`（引擎 skills.cpp:894），故命中/闪避是**掷点分岔**。
//   - T4401 **统计带**（主力断言）：40 次独立攻击，闪避次数落在 Binomial(40, 0.5)
//     的合理带 [12, 28]（μ=20、σ=√10≈3.16 → ±2.5σ ≈ [12,28]）。这才是"50%闪避"
//     的正确断言口径——单次掷点不能证明概率档位。
//   - T4402 **门闭对照**：不带魂印 → 40 次攻击**从不**闪避（闪避次数 = 0）。
//   - T4403 **每回合重注**：连续两回合各打一次，闪避率在第 2 回合仍生效
//     （ws 每回合 reset → 若不重注，第 2 回合 dodge_rate 归零 → 不闪避）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {50,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **概率断言法**：单次掷点分岔只能证明"有一次闪避"，不能证明"50% 档"——
//   故主力断言用**多次攻击 + 统计带**（K194 619 的 Binomial 带同款思路）。
//   闪避计数口径 = 印主掉血量 == 0 的回合数。
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

constexpr int kJi = 10001;     // 撞击：攻击载体
constexpr int kMaxHp = 99999;

SoulMark make_mark() {
    // DB args 原序 [50,1]：db[0]=50（"50%"，tips 自证）；db[1]=1 不消费（记档）。
    SoulMark sm(651, "K284 魂印", "test", EffectArgs(std::vector<int>{50, 1}));
    sm.monster_id = 2912;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "玄天影皇", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/1, kMaxHp, make_mark())
        : make_pet(1, "裸玄天影皇", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手打一次，印主掉血量（0 = 被闪避）。
int one_attack_loss(ScenarioBattle& b) {
    const int hp_before = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - b.ctx().getPet(0).hp;
}

// 统计 n 次攻击里的闪避次数（掉血 == 0 记为闪避）。
int count_dodges(int n, bool with_mark, int seed) {
    int dodges = 0;
    for (int i = 0; i < n; ++i) {
        ScenarioBattle b(make_party(mark_pet(with_mark)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(seed + i * 104729);
        if (one_attack_loss(b) == 0) {
            ++dodges;
        }
    }
    return dodges;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4400 闪避生效：掉血 == 0（被闪避）══
    {
        // 找一个闪避发生的种子（探针标定）。
        int dodge_seed = -1;
        for (int s = 1; s <= 200 && dodge_seed < 0; ++s) {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            b.ctx().seed_rng(s);
            if (one_attack_loss(b) == 0) {
                dodge_seed = s;
            }
        }
        REQUIRE(dodge_seed > 0);
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(dodge_seed);
        CHECK_EQ(one_attack_loss(b), 0,
                 "T4400 651：闪避发生 → 印主未掉血（seed=" + std::to_string(dodge_seed) + "）");
        std::cout << "  [T4400] 闪避生效 通过\n";
    }

    // ══ T4401 统计带：40 次攻击，闪避次数 ∈ [12, 28]（Binomial(40,0.5) ±2.5σ）══
    {
        const int dodges = count_dodges(40, /*with_mark=*/true, /*seed=*/20261021);
        CHECK(dodges >= 12 && dodges <= 28,
              "T4401 651：40 次攻击闪避次数落 Binomial(40,0.5) 带 [12,28]，实测 "
                  + std::to_string(dodges));
        std::cout << "  [T4401] 闪避统计带 通过 (dodges=" << dodges << "/40)\n";
    }

    // ══ T4402 门闭对照：不带魂印 → 从不闪避 ══
    {
        const int dodges = count_dodges(40, /*with_mark=*/false, /*seed=*/20261021);
        CHECK_EQ(dodges, 0,
                 "T4402 651：对照——不带魂印时 40 次攻击从不闪避（闪避确来自本魂印）");
        std::cout << "  [T4402] 无魂印对照 通过\n";
    }

    // ══ T4403 每回合重注：第 2 回合闪避率仍生效 ══
    {
        // 找两个都能闪避的种子（同局连续两回合）。
        int seed = -1;
        for (int s = 1; s <= 400 && seed < 0; ++s) {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            b.ctx().seed_rng(s);
            const int r1 = one_attack_loss(b);
            const int r2 = one_attack_loss(b);
            if (r1 == 0 && r2 == 0) {
                seed = s;
            }
        }
        REQUIRE(seed > 0);
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(seed);
        CHECK_EQ(one_attack_loss(b), 0, "T4403 第 1 回合闪避");
        CHECK_EQ(one_attack_loss(b), 0,
                 "T4403 651：第 2 回合仍闪避（ws 每回合 reset → ROUND_START 必须重注，"
                 "seed=" + std::to_string(seed) + "）");
        std::cout << "  [T4403] 每回合重注 通过\n";
    }

    std::cout << "scenario_4400 PASS (K284 概率闪避族)" << std::endl;
    return 0;
}
