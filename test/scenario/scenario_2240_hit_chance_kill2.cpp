#include <utils/sbs_rand.h>
// 场景 2240 —— J130 命中概率秒杀族（1 宏，moves_lib/j_hit_chance_kill2.cpp）
//（2026-10-04，worktree ../agents/j130；号段 2240-2249 → T2240-T2241 两块）
//
// 断言口径：
//   - T2240 100% 载体（跪地求饶 28186 [100] 属性技 prio0；链 72=miss 自罚未实现哑、
//     必中下永不触发）：
//     a) 必中 → 对手体力归零（秒杀落地，foe hp=0）；
//     b) miss 门负例：雷光振杀 19507 [10] 纯链 accuracy=0 强制 miss → 对手满血存活
//        （"命中后"门关）。
//   - T2241 统计侧证：缔灵之触 36311 [15] 纯链 物140（atk=1 红伤 1-2 不致死），
//     100 局各 1 回合——死亡数 ∈ [3,30]（B(100,0.15) 双侧尾 <0.3%）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 基线
constexpr int kGuiDi   = 28186;  // 跪地求饶 [691,72]|[100]：属性技（100% 载体）
constexpr int kLeiGuang= 19507;  // 雷光振杀 [691]|[10]：特130（miss 门负例载体）
constexpr int kDiLing  = 36311;  // 缔灵之触 [691]|[15]：物140（统计载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

ElfPet mk_self(std::array<int, 5> skills, int accuracy = 100, bool must_hit = true) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, kSelfHp);
    for (Skills& s : p.skills) {
        s.accuracy = accuracy;
        // ⚠️ must_hit 会盖过 accuracy=0（J108 记档：accuracy 造 miss 需同清 must_hit）。
        s.must_hit = must_hit ? 1 : 0;
    }
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, kFoeHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 跑 1 回合己方用 idx 技，返回对手是否死亡。
bool run_kill_trial(std::array<int, 5> skills, int accuracy, unsigned int seed, bool must_hit = true) {
    ScenarioBattle b(make_party(mk_self(skills, accuracy, must_hit)),
                     make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
    b.ctx().seed_rng(seed);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return b.ctx().getPet(1).hp <= 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2240.a 100% 秒杀（跪地求饶，必中）══
    {
        ScenarioBattle b(make_party(mk_self({kGuiDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0,
              "T2240.a 691：100% 命中后秒杀（foe hp 实测 " +
                  std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T2240.a] 100% 秒杀 通过\n";
    }

    // ══ T2240.b miss 门负例（雷光振杀 accuracy=0 + 清 must_hit → 必 miss）══
    {
        ScenarioBattle b(make_party(mk_self({kLeiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*accuracy=*/0, /*must_hit=*/false)),
                         make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp > 0,
              "T2240.b 691 miss 门：miss 不秒杀（foe hp 实测 " +
                  std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T2240.b] miss 门负例 通过\n";
    }

    // ══ T2241 15% 统计侧证（缔灵之触 100 局）══
    {
        constexpr int kTrials = 100;
        int deaths = 0;
        SbsRng trial_seeds(20261004);
        for (int t = 0; t < kTrials; ++t) {
            if (run_kill_trial({kDiLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, trial_seeds.next())) {
                ++deaths;
            }
        }
        CHECK(deaths >= 3 && deaths <= 30,
              "T2241 691 统计带：100 局死亡 " + std::to_string(deaths) +
                  " 应落在 B(100,0.15) 双侧尾 <0.3% 的 [3,30]");
        std::cout << "  [T2241] 15% 统计侧证 通过（死亡 " << deaths << "/" << kTrials
                  << "）\n";
    }

    std::cout << "scenario_2240 PASS (J130 命中概率秒杀族 691)" << std::endl;
    return 0;
}
