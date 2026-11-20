#include <utils/sbs_rand.h>
// 场景 2120 —— J118 升降槽计数固伤族（2 宏，moves_lib/j_sum_slot_fixed.cpp）
//（2026-10-03，worktree ../agents/j118；号段 2120-2129 → T2120-T2121 两块）
//
// 断言口径（J96 542 同款：atk=1 红伤 1-5 点 + 精确粉伤带宽头部余量）：
//   - 713（潮腾雾升 [60] 特130 prio0 纯链）：
//     a) 自身 +2/+1（Σ=3）→ 粉 180 → foe_loss ∈ [181,188]
//     b) 自身 +2/速 -1（Σ=2——**下降不计**）→ 粉 120 → foe_loss ∈ [121,128]
//     c) 无预设 → 无粉 → foe_loss ≤ 8
//   - 1772（凌风逸天宵 [1243✓,191,1439,1772]|[3,50,50] 特160 prio0；1243=先制+1
//     不扰伤害带、191 无异常不发、1439 哑——⚠️ 链激活风险用带宽头部对冲）：
//     a) 对手 -2/-1（Σ=3）→ 粉 150 → foe_loss ∈ [151,158]
//     b) 对手 -2/速 +1（Σ=2——**提升不计**）→ 粉 100 → foe_loss ∈ [101,108]
//     c) 无预设 → 无粉 → foe_loss ≤ 8
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
constexpr int kChaoTeng= 34036;  // 潮腾雾升 [713]|[60]：特130（713 载体，prio0）
constexpr int kLingFeng= 35814;  // 凌风逸天宵 […,1772]|[…,50]：特160（1772 载体，prio0）
constexpr int kMaxHp   = 5000;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 99999);
    accuracy_100(p);
    return p;
}

// 跑一回合"己方用 idx 技打 NONE 对手"，返回对手掉血。
int run_one_hit(std::array<int, 5> self_skills, int skill_idx,
                const std::vector<std::pair<int, int>>& self_boosts,
                const std::vector<std::pair<int, int>>& foe_drops) {
    ScenarioBattle b(make_party(mk_self(self_skills)), make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
    b.start();
    BattleContext& ctx = b.ctx();
    for (const auto& [slot, lv] : self_boosts) {
        (void)stat_change(&ctx, 0, slot, lv);
    }
    for (const auto& [slot, lv] : foe_drops) {
        (void)stat_change(&ctx, 1, slot, lv);
    }
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261003);

    // ══ T2120 713 自身提升等级总和 × 60（潮腾雾升）══
    {
        const std::array<int, 5> kit = {kChaoTeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
        const int loss_a = run_one_hit(kit, 0, {{0, 2}, {2, 1}}, {});
        CHECK(loss_a >= 181 && loss_a <= 188,
              "T2120.a 713：Σ=3×60=180 粉 + 红伤带（实测 " + std::to_string(loss_a) + "）");
        const int loss_b = run_one_hit(kit, 0, {{0, 2}, {4, -1}}, {});
        CHECK(loss_b >= 121 && loss_b <= 128,
              "T2120.b 713：下降不计 Σ=2×60=120 粉（实测 " + std::to_string(loss_b) + "）");
        const int loss_c = run_one_hit(kit, 0, {}, {});
        CHECK(loss_c <= 8,
              "T2120.c 713 对照：无提升无粉（实测 " + std::to_string(loss_c) + "）");
        std::cout << "  [T2120] 713 提升总和固伤 通过\n";
    }

    // ══ T2121 1772 对手下降等级总和 × 50（凌风逸天宵）══
    {
        const std::array<int, 5> kit = {kLingFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
        const int loss_a = run_one_hit(kit, 0, {}, {{0, -2}, {2, -1}});
        CHECK(loss_a >= 151 && loss_a <= 158,
              "T2121.a 1772：Σ=3×50=150 粉 + 红伤带（实测 " + std::to_string(loss_a) + "）");
        const int loss_b = run_one_hit(kit, 0, {}, {{0, -2}, {4, 1}});
        CHECK(loss_b >= 101 && loss_b <= 108,
              "T2121.b 1772：提升不计 Σ=2×50=100 粉（实测 " + std::to_string(loss_b) + "）");
        const int loss_c = run_one_hit(kit, 0, {}, {});
        CHECK(loss_c <= 8,
              "T2121.c 1772 对照：无下降无粉（实测 " + std::to_string(loss_c) + "）");
        std::cout << "  [T2121] 1772 下降总和固伤 通过\n";
    }

    std::cout << "scenario_2120 PASS (J118 升降槽计数固伤族 713/1772)" << std::endl;
    return 0;
}
