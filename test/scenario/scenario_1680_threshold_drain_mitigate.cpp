// 场景 1680 —— J78 阈值吸取/减伤窗族（1104/665，moves_lib/j_threshold_drain_mitigate.cpp）
//（2026-10-03，worktree ../agents/j78；号段 1680-1689 → T1680-T1681 两块）
//
// 断言口径：
//   - 1104 阈值门（31492 [300,3] 特150）：atk=1 红 ≪300 → 吸取：粉探针恰 1 笔
//     ==max/3=33332 且对手 hp 差同值；atk=3000 红 ≥300 → 门关无吸取。
//   - 665 阈值减伤窗（34008 [1022,665]|[200,200,2,200]，1022 对手无提升即惰性）：
//     R1 低伤命中挂窗 → R2 对手拍打 atk3000 被 -200（GUARD_DETECT 探针差分）；
//     R3 窗外（InRounds 2 回合 [r0,r0+1]）→ 不再减。
//   粉 FINAL 探针/GUARD_DETECT 探针均 075/T1491 同款。
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
constexpr int kPaiDa  = 10004;   // 拍打（对手攻击刀 / 我方填充）
constexpr int kShanShi = 31492;  // [1104]|[300,3] 特150（阈值吸取）
constexpr int kLinJia = 34008;   // [1022,665]|[200,200,2,200] 特85（阈值减伤窗）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "阈值族", skills, speed, atk, hp);
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

    // ══ T1680 1104 阈值吸取（31492 [300,3]）══
    {
        {   // (a) atk=1 红≪300 → 吸取 max/3
            ScenarioBattle b(
                make_party(self_pet({kShanShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.resolvedDamage.final < 300,
                  "T1680.a 前置：红 " + std::to_string(ctx.resolvedDamage.final) +
                      " < 300");
            CHECK_EQ(static_cast<int>(pinks.size()), 1, "T1680.a 恰 1 笔粉伤");
            CHECK_EQ(pinks.back(), 99998 / 3,
                     "T1680.a 1104：吸取 max/3=33332（实测 " + std::to_string(pinks.back()) +
                         "）");
            CHECK_EQ(99998 - ctx.getPet(1).hp, ctx.resolvedDamage.final + pinks.back(),
                     "T1680.a 对手 hp 差 == 红 + 吸取量");
        }
        {   // (b) atk=3000 红≥300 → 门关
            ScenarioBattle b(
                make_party(self_pet({kShanShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/3000)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.resolvedDamage.final >= 300,
                  "T1680.b 前置：红 " + std::to_string(ctx.resolvedDamage.final) +
                      " ≥ 300");
            CHECK_EQ(static_cast<int>(pinks.size()), 0,
                     "T1680.b 1104 门：高伤无吸取");
        }
        std::cout << "  [T1680] 1104 阈值吸取 通过\n";
    }

    // ══ T1681 665 阈值减伤窗（34008 [200,2,200]）══
    {
        ScenarioBattle b(
            make_party(self_pet({kLinJia, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                /*speed=*/300, /*atk=*/1, /*hp=*/99999)),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                               /*speed=*/100, /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        int pre_guard = -1;
        ctx.register_damage_effect(
            DamagePhase::GUARD_DETECT, 0, DamageEffectCategory::DETECT,
            [&pre_guard](BattleContext* c, int) {
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId == 0 && d.isRed && d.final > 0) {
                    pre_guard = d.final;
                }
            });
        // R1：低伤命中挂窗（atk=1 红≪200）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // R2（窗内）：对手拍打 → -200
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(pre_guard > 200, "T1681.a 前置：减前值已抓到");
        CHECK_EQ(pre_guard - ctx.resolvedDamage.final, 200,
                 "T1681.a 665 窗内：受伤 -200（pre=" + std::to_string(pre_guard) + "）");
        // R3（窗外 [r0,r0+1] 已过）：不再减
        pre_guard = -1;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(pre_guard > 0, "T1681.b 前置：R3 减前值已抓到");
        CHECK_EQ(pre_guard - ctx.resolvedDamage.final, 0,
                 "T1681.b 665 窗外：不再减伤（pre=" + std::to_string(pre_guard) + "）");
        std::cout << "  [T1681] 665 阈值减伤窗 通过\n";
    }

    std::cout << "scenario_1680 PASS (J78 阈值吸取/减伤窗族)" << std::endl;
    return 0;
}
