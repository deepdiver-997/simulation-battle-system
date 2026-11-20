// 场景 1380 —— J52 计数输出族（1328/768/848，moves_lib/j_count_output.cpp）
//（2026-10-03，worktree ../agents/j52；号段 1380-1389 → T1380-T1382 三块）
//
// 断言口径：
//   - 1328 概率递增（32909 星辰腐朽 [10,3,10,30]）：概率梯度走审计槽**无随机精确
//     断言**（10/20/30/30/30，F9 782 首创）+ ×3 乘伤统计带侧证（atk=1000 同局
//     proc/plain 均值比 ∈[2.4,3.7]；红伤 85~100% 浮动 → 理论带 [2.55,3.53]）；
//     换技不断账（782 口径：uses 按技能分账、跨技能持续）。
//   - 768 逐异常种数（37111 破壁蚀崖 [200]）：loss = 红 + 200×种数，全确定；
//     异常选无回合末伤害的 麻痹0/害怕6/疲惫7（烧伤/中毒有 tick 会污染精确差分）。
//   - 848 逐负槽种数（35097 理性追逐 [50]）：loss = 红 + 50×负槽槽数，全确定；
//     幅度不计种（-3 仍=1 种）。
//   atk=1 消红噪声；loss 与 resolvedDamage.final 差分即附加粉（T1372 口径）。
#include <any>
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
constexpr int kXingChen = 32909;  // 星辰腐朽 [1328]|[10,3,10,30] 物130（1328 梯度）
constexpr int kPoBi    = 37111;   // 破壁蚀崖 [768]|[200] 物130（768 逐异常）
constexpr int kLiXing  = 35097;   // 理性追逐 [848]|[50] 物140（848 逐负槽）

// 审计槽键（与 j_count_output.cpp 对表；只读）
constexpr int k1328ChanceAuditBase = 990152200;
constexpr int k1328ProcAuditBase = 990152300;

int audit_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "计数族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1380 1328 概率递增 ×{1} 乘伤（32909 [10,3,10,30]）══
    {
        {   // (a) 梯度精确（审计槽）：10/20/30/30/30（首用=base，封顶 cap）
            ScenarioBattle b(make_party(self_pet({kXingChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int expect[5] = {10, 20, 30, 30, 30};
            for (int r = 0; r < 5; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                CHECK_EQ(audit_int(ctx, k1328ChanceAuditBase + 0), expect[r],
                         "T1380.a 1328 第 " + std::to_string(r + 1) +
                             " 用申报概率（审计槽，期望 " + std::to_string(expect[r]) + "）");
            }
            std::cout << "  [T1380.a] 1328 概率梯度 10/20/30/30/30 通过\n";
        }
        {   // (b) ×3 乘伤统计带：同局 proc 轮红伤 / plain 轮红伤均值 ∈ [2.4, 3.7]
            int total_proc = 0;
            for (int game = 0; game < 8; ++game) {
                ScenarioBattle b(
                    make_party(self_pet({kXingChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                        /*speed=*/300, /*atk=*/1000)),
                    make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                       /*speed=*/100, /*atk=*/1, /*hp=*/999999)));
                b.start();
                BattleContext& ctx = b.ctx();
                std::vector<int> proc_samples;
                double plain_sum = 0.0;
                int plain_n = 0;
                for (int r = 0; r < 6; ++r) {
                    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                    b.drive();
                    const bool proc = audit_int(ctx, k1328ProcAuditBase + 0) != 0;
                    const int red = ctx.resolvedDamage.final;
                    if (proc) {
                        ++total_proc;
                        proc_samples.push_back(red);
                    } else {
                        plain_sum += red;
                        ++plain_n;
                    }
                }
                if (total_proc > 0 && !proc_samples.empty() && plain_n > 0) {
                    const double plain_mean = plain_sum / plain_n;
                    // 本局所有 proc 轮逐一比对带（存首尾即可：循环内断言）
                    for (const int red : proc_samples) {
                        const double ratio = red / plain_mean;
                        CHECK(ratio >= 2.4 && ratio <= 3.7,
                              "T1380.b 1328 ×3 乘伤带（ratio=" + std::to_string(ratio) +
                                  "，red=" + std::to_string(red) + "）");
                    }
                }
                proc_samples.clear();
            }
            CHECK(total_proc > 0,
                  "T1380.b 前置：8 局×6 用至少一次命中（期望 1.5/局，P 全 miss≈5e-6）");
            std::cout << "  [T1380.b] 1328 ×3 乘伤统计带通过（proc " << total_proc << " 次）\n";
        }
        {   // (c) 换技不断账：1328 → 拍打（非载体，不计用）→ 1328，uses=2 → 申报 20
            ScenarioBattle b(make_party(self_pet({kXingChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 1328
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 1328
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit_int(ctx, k1328ChanceAuditBase + 0), 20,
                     "T1380.c 1328 换技不断账：跨拍打后第 2 次 1328 申报 20（非重置的 10）");
        }
        std::cout << "  [T1380] 1328 概率递增×乘伤 通过\n";
    }

    // ══ T1381 768 逐异常种数（37111 [200]；无 tick 异常池）══
    {
        {   // (a) 对照：无异常 → loss = 红
            ScenarioBattle b(make_party(self_pet({kPoBi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final,
                     "T1381.a 768 对照：无异常无固伤（loss=" + std::to_string(loss) + "）");
        }
        {   // (b) 2 种（麻痹+害怕）→ loss = 红 + 400
            ScenarioBattle b(make_party(self_pet({kPoBi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 400,
                     "T1381.b 768 两异常 → +400（loss=" + std::to_string(loss) + "）");
        }
        {   // (c) 3 种（+疲惫）→ +600；同种重复施加不计种（麻痹×2+害怕 → 仍 +400）
            ScenarioBattle b(make_party(self_pet({kPoBi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear), 9, 0);
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fatigue), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 600,
                     "T1381.c 768 三种 → +600，同种重复不计（loss=" + std::to_string(loss) +
                         "）");
        }
        std::cout << "  [T1381] 768 逐异常种数 通过\n";
    }

    // ══ T1382 848 逐负槽种数（35097 [50]）══
    {
        {   // (a) 对照：无下降 → loss = 红
            ScenarioBattle b(make_party(self_pet({kLiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final,
                     "T1382.a 848 对照：无下降无固伤（loss=" + std::to_string(loss) + "）");
        }
        {   // (b) 2 槽各 -1 → loss = 红 + 100
            ScenarioBattle b(make_party(self_pet({kLiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 1, 2, -1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 100,
                     "T1382.b 848 两负槽 → +100（loss=" + std::to_string(loss) + "）");
        }
        {   // (c) 幅度不计种：单槽 -3 → 仍 1 种 → +50
            ScenarioBattle b(make_party(self_pet({kLiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -3) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 50,
                     "T1382.c 848 单槽 -3 仍 1 种 → +50（loss=" + std::to_string(loss) +
                         "）");
        }
        std::cout << "  [T1382] 848 逐负槽种数 通过\n";
    }

    std::cout << "scenario_1380 PASS (J52 计数输出族)" << std::endl;
    return 0;
}
