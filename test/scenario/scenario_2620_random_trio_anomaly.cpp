#include <utils/sbs_rand.h>
// 场景 2620 —— J164 随机三异常族（74，moves_lib/j_random_trio_anomaly.cpp）
//（2026-10-04，worktree ../agents/j164；号段 2620-2629 → T2620 两块）
//
// 断言口径：纯链载体 奇幻光芒 11140（30% 概率——统计带断言）：
//   100 种子独立对局扫描：落码率 ∈ [20,40]（30%±2σ，σ≈4.6）；落码恒 ∈ {1,2,5}
//   且至多 1 种（三选一）；未落种子对手无任何 0..9 异常（概率门外无副作用）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kQiHuan = 11140;   // 奇幻光芒：特技 纯链 [74]（零参）
constexpr int kPaDa   = 10004;   // 拍打：filler

constexpr int kPoison    = static_cast<int>(AbnormalStatusId::Poison);     // 1
constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

// 一场种子 s 的对局：返回对手 {1,2,5} 内的落码数（及是否落了池外码）。
void run_one(int seed, int& pool_hits, bool& outside_anomaly) {
    ScenarioBattle b(make_party(make_pet(1, "己方", std::array<int, 5>{kQiHuan, kPaDa,
                                                                             kPaDa, kPaDa, kPaDa},
                                         300, 1, 99999)),
                     make_party(make_pet(2, "对手", std::array<int, 5>{kPaDa, kPaDa, kPaDa,
                                                                             kPaDa, kPaDa},
                                         100, 1, 99999)));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(static_cast<unsigned>(seed));
    CHECK_EQ(ctx.rng().state(), static_cast<std::uint64_t>(seed),
             "T2620 每个对局使用本次试验指定的种子");
    for (Skills& s : ctx.seerRobot[0].elfPets[0].skills) {
        (void)s;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    pool_hits = 0;
    outside_anomaly = false;
    for (int code = 0; code <= 9; ++code) {
        if (!ctx.has_active_abnormal_status(1, code)) {
            continue;
        }
        if (code == kPoison || code == kBurn || code == kFrostbite) {
            ++pool_hits;
        } else {
            outside_anomaly = true;
        }
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2620 74 30% 统计带 + 落码池校验（100 种子扫描）══
    {
        int landed = 0;
        int outside = 0;
        int multi = 0;
        for (int s = 1; s <= 100; ++s) {
            int hits = 0;
            bool out = false;
            run_one(s, hits, out);
            if (hits > 0) {
                ++landed;
            }
            if (hits > 1) {
                ++multi;
            }
            if (out) {
                ++outside;
            }
        }
        CHECK(landed >= 20 && landed <= 40,
              "T2620 74：30% 统计带（100 种子落地 " + std::to_string(landed) +
                  " ∈ [20,40]）");
        CHECK_EQ(outside, 0, "T2620 74：落码恒 ∈ {1,2,5}（池外零落）");
        CHECK_EQ(multi, 0, "T2620 74：三选一（至多 1 种）");
        std::cout << "  [T2620] 74 统计带 通过 (landed=" << landed << "/100)\n";
    }

    std::cout << "scenario_2620 PASS (J164 随机三异常族)" << std::endl;
    return 0;
}
