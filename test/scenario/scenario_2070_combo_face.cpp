// 场景 2070 —— 连击面批产（484/612/1454/1172/1685，moves_lib/j_combo_face.cpp）
//（2026-10-03 白天专项；号段 2070-2079 → T2070-T2074 五块）
//
// 断言口径（连击次数 N 直接读 ctx.resolvedDamage.hitCount——单次结算 ×N 的权威记录）：
//   - 484 跨使用递增（14226 [5,1,10]，纯链）：必中 → use1 N=5、use2 N=6、use3 N=7
//     （每次命中后 +{1}，封顶 {2}；全确定）。
//   - 612 概率多段（18134 [60,5]）：6 局 N ∈ {1,5}；≥1 局 5 段（P 全歪≈0.4%）。
//   - 1454 逐段概率固伤（34009 [5,15,10,50]）：5 攻击 N ∈ [5,15]；粉探针全 ==50、
//     ≥1 笔（期望 5 笔，P 全歪≈0.5%）。
//   - 1172 逐段概率自异常（30762 [1,2,6,30,1]）：5 攻击 N ∈ [2,6]；自身中毒
//     （期望 6 段 proc，P 全歪≈0.08%）。
//   - 1685 异常提上限（35058 [2,4,6]）：对手中毒 → 10 攻击 N ∈ [2,6] 且 ≥1 次 >4
//     （P 全歪≈0.6%）；对照无异常 → N ∈ [2,4]（确定界）。
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
constexpr int kYueYa   = 14226;   // 月牙之刃 [484]|[5,1,10] 物25（递增连击）
constexpr int kKuaiSu  = 18134;   // 快速斩击 [612]|[60,5] 物50（概率 5 段）
constexpr int kTangYuan = 34009;  // 汤圆连打 [1454]|[5,15,10,50] 物20（逐段固伤）
constexpr int kChiYan  = 30762;   // 炽焰狙击 [1172]|[1,2,6,30,1,...] 特30（逐段自异常）
constexpr int kGuangNeng = 35058; // 光能连击 [1685]|[2,4,6] 物30（异常提上限）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000, int hp = 99999) {
    ElfPet p = make_pet(1, "连击面", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 999999) {
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

    // ══ T2070 484 跨使用递增（14226 [5,1,10]）══
    {
        ScenarioBattle b(make_party(self_pet({kYueYa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect[3] = {5, 6, 7};
        for (int r = 0; r < 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.resolvedDamage.hitCount, expect[r],
                     "T2070 484 第 " + std::to_string(r + 1) + " 用连击数 == " +
                         std::to_string(expect[r]));
        }
        std::cout << "  [T2070] 484 递增连击 5/6/7 通过\n";
    }

    // ══ T2071 612 概率多段（18134 [60,5]）══
    {
        int procs = 0;
        for (int game = 0; game < 6; ++game) {
            ScenarioBattle b(
                make_party(self_pet({kKuaiSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int n = ctx.resolvedDamage.hitCount;
            CHECK(n == 1 || n == 5, "T2071 612：N ∈ {1,5}（实测 " + std::to_string(n) + "）");
            if (n == 5) {
                ++procs;
            }
        }
        CHECK(procs > 0, "T2071 前置：6 局至少一次 60% 命中（P 全歪≈0.4%）");
        std::cout << "  [T2071] 612 概率多段 通过（6 局 5 段 " << procs << " 次）\n";
    }

    // ══ T2072 1454 逐段概率固伤（34009 [5,15,10,50]）══
    {
        ScenarioBattle b(make_party(self_pet({kTangYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        std::vector<int> pinks;
        install_pink_probe(ctx, pinks);
        int total_hits = 0;
        for (int r = 0; r < 5; ++r) {   // 汤圆连打 max_pp=5
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int n = ctx.resolvedDamage.hitCount;
            CHECK(n >= 5 && n <= 15,
                  "T2072 1454 第 " + std::to_string(r + 1) + " 击 N ∈ [5,15]（实测 " +
                      std::to_string(n) + "）");
            total_hits += n;
        }
        int procs = 0;
        for (const int v : pinks) {
            CHECK_EQ(v, 50, "T2072 1454：粉伤笔值 == 50（实测 " + std::to_string(v) + "）");
            ++procs;
        }
        CHECK(procs >= 1, "T2072 前置：5 攻击 ≥1 笔 10% proc（期望 " +
                              std::to_string(total_hits / 10) + "，P 全歪≈0.5%）");
        std::cout << "  [T2072] 1454 逐段固伤 通过（" << total_hits << " 段 proc " << procs
                  << " 笔）\n";
    }

    // ══ T2073 1172 逐段概率自异常（30762 [1,2,6,30,1]）══
    {
        ScenarioBattle b(make_party(self_pet({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 5; ++r) {   // 炽焰狙击 max_pp=5
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int n = ctx.resolvedDamage.hitCount;
            CHECK(n >= 2 && n <= 6,
                  "T2073 1172 第 " + std::to_string(r + 1) + " 击 N ∈ [2,6]（实测 " +
                      std::to_string(n) + "）");
        }
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T2073 1172：自身中毒（期望 ~6 段 proc，P 全歪≈0.08%）");
        std::cout << "  [T2073] 1172 逐段自异常 通过\n";
    }

    // ══ T2074 1685 异常提上限（35058 [2,4,6]）══
    {
        {   // (a) 对手中毒 → 上限提至 6
            ScenarioBattle b(
                make_party(self_pet({kGuangNeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Poison), 9, 0);
            int max_seen = 0;
            for (int r = 0; r < 10; ++r) {   // 光能连击 max_pp=10
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int n = ctx.resolvedDamage.hitCount;
                CHECK(n >= 2 && n <= 6,
                      "T2074.a 1685 第 " + std::to_string(r + 1) + " 击 N ∈ [2,6]（实测 " +
                          std::to_string(n) + "）");
                max_seen = std::max(max_seen, n);
            }
            CHECK(max_seen > 4,
                  "T2074.a 1685：异常状态上限提至 6（10 击最大 " + std::to_string(max_seen) +
                      "，P 全 ≤4≈0.6%）");
        }
        {   // (b) 对照：对手无异常 → N ∈ [2,4]（确定界）
            ScenarioBattle b(
                make_party(self_pet({kGuangNeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 5; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int n = ctx.resolvedDamage.hitCount;
                CHECK(n >= 2 && n <= 4,
                      "T2074.b 1685 门：无异常 N ∈ [2,4]（实测 " + std::to_string(n) + "）");
            }
        }
        std::cout << "  [T2074] 1685 异常提上限 通过\n";
    }

    std::cout << "scenario_2070 PASS (连击面批产 484/612/1454/1172/1685)" << std::endl;
    return 0;
}
