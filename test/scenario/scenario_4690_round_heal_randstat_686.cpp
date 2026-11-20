#include <utils/sbs_rand.h>
// 场景 4690 —— K313 回合末回血+随机三项族（686，soul_lib/k313_round_heal_randstat.cpp）
//（2026-10-21，worktree ../agents/k313；号段 4690-4699 → T4690-T4692 三块）
//
// 断言口径（686「每个战斗阶段结束时恢复200HP，随机使自身3项属性+1」）：
//   - T4690 **回血生效**：过 ROUND_END → 自身恢复 200HP（精确；回血是整数无浮动）。
//   - T4691 **随机 3 项互不相同**：多种子循环（20 局）——每局恰有 **3 个**槽被 +1
//     且**无槽 ≥ +2**（"3项"必是 3 个互异槽；若抽样落同槽则只 1~2 槽被加）。
//     ⚠️ **随机效果必须多种子采样**（K267 645 教训：单一固定种子只覆盖一个抽样点，
//     破坏去重的实现可能恰好在那个点不撞槽）。
//   - T4692 **增量由行参驱动（参数化）**：用 db=[200,2,3] 的变体 → 恰 2 个槽被 +3
//     （把"读 db[1] 项数 / db[2] 增量"变成可观测行为）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **回血需要空间**：满血时回血不涨体力 → 先把印主压到缺口 1000 再过桶。
// ⚠️ 属性断言读 **ctx.ability_levels[0][槽]**：stat_change 写本体
//   （battle_primitives.cpp:1402）+ 视图同步，两者同源。
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

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kMaxHp = 99999;
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;

SoulMark make_mark(int heal = 200, int count = 3, int delta = 1) {
    // DB args 原样 [200,3,1]：db[0]=回血量 / db[1]=随机项数 / db[2]=属性增量。
    SoulMark sm(686, "K313 魂印", "test",
                EffectArgs(std::vector<int>{heal, count, delta}));
    sm.monster_id = 2976;
    return sm;
}

ElfPet mark_pet(int heal = 200, int count = 3, int delta = 1) {
    ElfPet p = make_pet(1, "天玄界王", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(heal, count, delta));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 压印主体力留回血空间 → 过 ROUND_END → 返回 [回血量, 被加槽数, 最大槽值]。
struct Probe {
    int healed = 0;
    int boosted_slots = 0;
    int max_slot = 0;
};

Probe one_round(ScenarioBattle& b, int seed) {
    BattleContext& ctx = b.ctx();
    ctx.seerRobot[0].elfPets[0].hp = kMaxHp - 1000;   // 留 1000 缺口
    const int hp_before = ctx.getPet(0).hp;
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Probe pr;
    pr.healed = ctx.getPet(0).hp - hp_before;
    for (int i = 0; i < kSlots; ++i) {
        const int lv = ctx.ability_levels[0][i];
        if (lv > 0) {
            ++pr.boosted_slots;
        }
        if (lv > pr.max_slot) {
            pr.max_slot = lv;
        }
    }
    return pr;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4690 回血生效：恢复 200HP ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const Probe pr = one_round(b, /*seed=*/20261021);
        CHECK_EQ(pr.healed, 200,
                 "T4690 686：战斗阶段结束时恢复 200HP（实测 " + std::to_string(pr.healed) + "）");
        std::cout << "  [T4690] 回血生效 通过\n";
    }

    // ══ T4691 随机 3 项互不相同（多种子 20 局）══
    {
        int ok_slots = 0;     // 恰 3 槽被加
        int bad_slots = 0;    // 槽数不对（抽样落同槽 → 违反"3项"）
        int bad_max = 0;      // 有槽 ≥ +2（同槽被加两次）
        const int rounds = 20;
        for (int i = 0; i < rounds; ++i) {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            const Probe pr = one_round(b, /*seed=*/20261021 + i * 7919);
            if (pr.boosted_slots == 3) {
                ++ok_slots;
            } else {
                ++bad_slots;
            }
            if (pr.max_slot >= 2) {
                ++bad_max;
            }
        }
        CHECK_EQ(bad_slots, 0,
                 "T4691 686：'随机3项'必须是 3 个**互不相同**的槽——20 局中恰 3 槽的局数 "
                     + std::to_string(ok_slots) + "，异常局数 " + std::to_string(bad_slots));
        CHECK_EQ(bad_max, 0,
                 "T4691 686：无单槽被加到 +2 以上（同槽抽样会违反'3项'），异常局数 "
                     + std::to_string(bad_max));
        std::cout << "  [T4691] 随机3项互不相同 通过 (" << ok_slots << "/" << rounds
                  << " 局恰3槽)\n";
    }

    // ══ T4692 增量由行参驱动（参数化 db=[200,2,3]）══
    {
        int ok = 0;
        int bad = 0;
        const int rounds = 12;
        for (int i = 0; i < rounds; ++i) {
            ScenarioBattle b(make_party(mark_pet(/*heal=*/200, /*count=*/2, /*delta=*/3)),
                             make_party(plain_pet()));
            b.start();
            const Probe pr = one_round(b, /*seed=*/20261021 + i * 7919);
            // 期望：恰 2 个槽被加，且每个恰 +3（无槽 ≥ +4）
            if (pr.boosted_slots == 2 && pr.max_slot == 3) {
                ++ok;
            } else {
                ++bad;
            }
        }
        CHECK_EQ(bad, 0,
                 "T4692 686：项数 = db[1] = 2、增量 = db[2] = 3（每槽恰 +3、无槽 ≥ +4）"
                 "——12 局中合格 " + std::to_string(ok) + "，异常 " + std::to_string(bad));
        std::cout << "  [T4692] 增量由行参驱动 通过 (" << ok << "/" << rounds << ")\n";
    }

    std::cout << "scenario_4690 PASS (K313 回合末回血+随机三项族)" << std::endl;
    return 0;
}
