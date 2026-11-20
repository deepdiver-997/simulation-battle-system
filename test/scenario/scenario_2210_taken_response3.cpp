#include <utils/sbs_rand.h>
// 场景 2210 —— J127 受击响应族三（1085 / 1617，moves_lib/j_taken_response3.cpp）
//（2026-10-04，worktree ../agents/j127；号段 2210-2219 → T2210-T2214 五块）
//
// 断言口径（J85/J14 场景同款）：
//   - 1085 走链短载体 黑色战殇 25275 [3,300,300]（伴生 48/47=自身强化/免异常窗，
//     对受击断言良性）：atk=1 裸伤 1~3 < 300 反打 300 固伤（foe hp 精确等差 -300）、
//     atk=3000 裸伤 ≥858 > 300 不反打（浮动带远离阈值，无随机）；窗长 3 靠 R4
//     过期不反打钉死（InRounds [R1..R3]，先手）；
//   - 1617 主干/兜底走 chance=100 载体 黑暗沉沦 27588 [3,100,6,1]（害怕；100% 短路
//     不耗 rand）：对手 ANOMALY 次免票强制"未触发"→ 自身六槽 +1（level_sum==6）；
//   - 1617 掷点两半走纯链载体 窃笑潜逃 36981 [3,70,15,1]：70% 种子校准
//     （序列 = [R1 我攻浮伤, R2 对手攻浮伤, R2 掷点]，must_hit 无 miss 消耗）。
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

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kHeiSe  = 25275;   // 黑色战殇：属 [48,47,1085] → 1085 [3,300,300]
constexpr int kHeiAn  = 27588;   // 黑暗沉沦：属 [191,1617,843] → 1617 [3,100,6,1]（害怕）
constexpr int kQieXiao= 36981;   // 窃笑潜逃：物130 纯链 [1617] → [3,70,15,1]（码15）
constexpr int kPaDa   = 10004;   // 拍打：物攻40（filler/对手攻击载体）

constexpr int kFear    = static_cast<int>(AbnormalStatusId::Fear);  // 6
constexpr int kCode15  = 15;     // 扩展异常码（窃笑潜逃切片）

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void foe_attacks(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// setup 直授：对手 ANOMALY 次免票（counts）——"未触发"强制器（场景 900 同款）。
void grant_foe_anomaly_counts_ticket(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9901,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/counts, /*source_effect_id=*/9901);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2210 1085 低于阈值反打 + 窗长3（黑色战殇 [3,300,300]，atk=1 闭式界）══
    {
        ScenarioBattle b(make_party(self_pet({kHeiSe, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（窗 [R1..R3]，先手）
        foe_attacks(b);     // R2 裸伤 1~3 < 300 → 反打 300 固伤
        CHECK(ctx.getPet(0).hp < 99999, "T2210 1085 前置：对手攻击落地");
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 300, "T2210 1085：低于阈值 → 附加 300 固伤反打");
        foe_attacks(b);     // R3 窗内 → 再反打
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 600, "T2210 1085：窗内第2次受击再反打");
        foe_attacks(b);     // R4 窗过期 → 不反打
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 600, "T2210 1085：窗过期后低伤不再反打（窗长=3）");
        std::cout << "  [T2210] 1085 低伤反打/窗长 通过\n";
    }

    // ══ T2211 1085 高于阈值不反打（atk=3000 裸伤 ≥858 > 300）══
    {
        ScenarioBattle b(make_party(self_pet({kHeiSe, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装
        foe_attacks(b);     // R2 高伤 → 不反打
        CHECK(ctx.getPet(0).hp < 99999, "T2211 1085 前置：对手攻击落地");
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T2211 1085：不低于阈值 → 无附加固伤");
        std::cout << "  [T2211] 1085 高伤不反打 通过\n";
    }

    // ══ T2212 1617 chance=100 主干（黑暗沉沦 [3,100,6,1]）══
    {
        ScenarioBattle b(make_party(self_pet({kHeiAn, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（100% 短路不耗 rand）
        foe_attacks(b);     // R2 受击 → 害怕必落
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T2212 1617：100% 主干 → 害怕落地");
        CHECK_EQ(level_sum(ctx, 0), 0, "T2212 1617：主干触发则兜底不触发");
        std::cout << "  [T2212] 1617 主干 通过\n";
    }

    // ══ T2213 1617 未触发兜底（对手 ANOMALY 次免票强制）══
    {
        ScenarioBattle b(make_party(self_pet({kHeiAn, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_counts_ticket(ctx, /*counts=*/1);
        self_skill(b, 0);   // R1 武装
        foe_attacks(b);     // R2 受击 → 申报被免 → 自身六槽 +1
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T2213 1617：次免票挡下申报");
        CHECK_EQ(level_sum(ctx, 0), 6, "T2213 1617 兜底：自身六槽全 +1（含命中）");
        std::cout << "  [T2213] 1617 兜底 通过\n";
    }

    // ══ T2214 1617 掷点两半（窃笑潜逃 [3,70,15,1] 纯链，70% 种子校准）══
    {
        int seed_hit = -1;
        int seed_fail = -1;
        for (int s = 1; s < 100000 && (seed_hit < 0 || seed_fail < 0); ++s) {
            sbs_srand(s);
            (void)sbs_rand();               // d1 = R1 我方攻击浮伤
            (void)sbs_rand();               // d2 = R2 对手攻击浮伤
            const int d3 = sbs_rand() % 100;  // d3 = R2 受击掷点
            if (seed_hit < 0 && d3 < 70) {
                seed_hit = s;
            }
            if (seed_fail < 0 && d3 >= 70) {
                seed_fail = s;
            }
        }
        REQUIRE(seed_hit > 0);
        REQUIRE(seed_fail > 0);
        {   // (a) 掷中：对手中码 15，无兜底
            ScenarioBattle b(make_party(self_pet({kQieXiao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_hit);
            self_skill(b, 0);   // R1 窃笑潜逃（物攻，浮伤耗 d1）
            foe_attacks(b);     // R2 受击掷点 d3 < 70 → 中
            CHECK(ctx.has_active_abnormal_status(1, kCode15),
                  "T2214.a 1617：掷中 → 对手异常码15");
            CHECK_EQ(level_sum(ctx, 0), 0, "T2214.a 1617：掷中无兜底");
        }
        {   // (b) 掷歪：无异常，自身六槽 +1
            ScenarioBattle b(make_party(self_pet({kQieXiao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_fail);
            self_skill(b, 0);
            foe_attacks(b);
            CHECK(!ctx.has_active_abnormal_status(1, kCode15),
                  "T2214.b 1617：掷歪 → 无异常");
            CHECK_EQ(level_sum(ctx, 0), 6, "T2214.b 1617：掷歪 → 自身六槽 +1");
        }
        std::cout << "  [T2214] 1617 掷点两半 通过 (seed " << seed_hit << "/" << seed_fail
                  << ")\n";
    }

    std::cout << "scenario_2210 PASS (J127 受击响应族三)" << std::endl;
    return 0;
}
