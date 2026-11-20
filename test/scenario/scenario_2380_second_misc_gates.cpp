#include <utils/sbs_rand.h>
// 场景 2380 —— J145 后手/兜底杂门（1736 / 1201 / 1318，moves_lib/j_second_misc_gates.cpp）
//（2026-10-04，worktree ../agents/j145；号段 2380-2389 → T2380-T2382 三块）
//
// 断言口径：
//   - 1736 走纯链载体 泥石吞没 35548 [2]（物150）：对手当回合技槽 PP 用**对照差账**
//     （同对局另一场我不带 1736 → 拍打 pp 只走正常 -1；带 1736 且我后手 → -3，
//     差恰 2）；先手对照（我提速）→ 差 0；
//   - 1201 走纯链载体 九霄雷鸣诀 30898 [100,0,2,1]（主干 100% 确定档）：主干落地
//     断 has_any_active_abnormal_status（码 0 落地非确定——速查记档，不断言具体码）；
//     兜底用对手 ANOMALY 次免票强制 + self speed -2 垫差（101→视图50 vs 100）：
//     先制 +1 → 视图151 → 我先手（SEER_ROBOT_1），窗 [R2,R3]、R4 过期回后手——
//     先手权字段判据（preemptive_right），窗长判别性三连；
//   - 1318 走纯链载体 星河洞天舞 33416 [10,1]（特160）：10% 种子校准两半
//     （序列=[施放浮伤, AFTER_ACTION 掷点]）：中 → 对手 hp==0；歪 → 六槽各 -1
//     （level_sum==-6，含命中）。
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

constexpr int kNiShi  = 35548;   // 泥石吞没：物150 纯链 [1736] → [2]
constexpr int kJiuXiao= 30898;   // 九霄雷鸣诀：物150 纯链 [1201] → [100,0,2,1]
constexpr int kXingHe = 33416;   // 星河洞天舞：特160 纯链 [1318] → [10,1]
constexpr int kPaDa   = 10004;   // 拍打：filler
constexpr int kSlotSpd = 4;      // 引擎速度等级槽

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void both_pass(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

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

    // ══ T2380 1736 后手额外扣 PP（对照差账=2；先手对照=0）══
    {
        int pp_control = -1;
        {   // 对照：不带 1736（我拍打），我后手 → 对手拍打 pp 正常 -1
            ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            both_act(b, 0);
            pp_control = ctx.seerRobot[1].elfPets[0].skills[0].pp;
        }
        {   // 带 1736、我后手 → 额外 -2
            ScenarioBattle b(make_party(self_pet({kNiShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            both_act(b, 0);
            const int pp_test = ctx.seerRobot[1].elfPets[0].skills[0].pp;
            CHECK_EQ(pp_control - pp_test, 2,
                     "T2380 1736：后手 → 对手当回合技槽 PP 额外 -2（对照差账）");
        }
        {   // 先手对照：我提速 300 → 先手，无额外
            ScenarioBattle b(make_party(self_pet({kNiShi, kPaDa, kPaDa, kPaDa, kPaDa},
                                                 /*speed=*/300)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            both_act(b, 0);
            const int pp_first = ctx.seerRobot[1].elfPets[0].skills[0].pp;
            CHECK_EQ(pp_first, pp_control,
                     "T2380 1736：先手 → 无额外扣（对照相等）");
        }
        std::cout << "  [T2380] 1736 后手额外扣PP 通过\n";
    }

    // ══ T2381 1201 主干确定档（九霄雷鸣诀 [100,0,2,1]）══
    {
        ScenarioBattle b(make_party(self_pet({kJiuXiao, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*speed=*/101)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我先手施放 → 100% 申报落地
        CHECK(ctx.has_any_active_abnormal_status(1),
              "T2381 1201：100% 主干落地（码 0 落地非确定，断言任一异常）");
        REQUIRE(stat_drop(&ctx, 0, kSlotSpd, 2, -1) == StatDropResult::SUCCESS);
        both_act(b, 1);   // R2 速度垫差后：无兜底 → 我后手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T2381 1201：主干落地 → 无先制兜底（速度垫差后我后手）");
        std::cout << "  [T2381] 1201 主干通过\n";
    }

    // ══ T2382 1201 兜底先制窗（对手次免票强制未触发；[2] 窗 R2/R3、R4 过期）══
    {
        ScenarioBattle b(make_party(self_pet({kJiuXiao, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*speed=*/101)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_counts_ticket(ctx, /*counts=*/1);
        both_act(b, 0);   // R1 我先手施放 → 申报被票挡 → 兜底武装
        CHECK(!ctx.has_any_active_abnormal_status(1), "T2382 前置：次免票挡下申报");
        REQUIRE(stat_drop(&ctx, 0, kSlotSpd, 2, -1) == StatDropResult::SUCCESS);
        both_act(b, 1);   // R2 窗内：先制+1 → 视图 151 vs 100 → 我先手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T2382 1201：兜底 R2 → 先制+1 翻转先手");
        both_act(b, 1);   // R3 窗内第 2 回合 → 仍我先手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T2382 1201：兜底 R3 → 窗内仍先手（窗长=2）");
        both_act(b, 1);   // R4 窗过期 → 回后手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T2382 1201：R4 窗过期 → 回后手");
        std::cout << "  [T2382] 1201 兜底先制窗 通过\n";
    }

    // ══ T2383 1318 两半（星河洞天舞 [10,1]）——运行期自校准 ══
    // 掷点消耗的 rand 序位随种子漂移（引擎伤害管线预抽不定），种子静态校准不可靠
    // ——改为循环种子实测：kill 种子=对手 hp100 被秒；drop 种子=存活且六槽 -1。
    {
        int seed_kill = -1;
        int seed_drop = -1;
        int drop_sum = 0;
        for (int s = 1; s < 400 && (seed_kill < 0 || seed_drop < 0); ++s) {
            if (seed_kill < 0) {
                ScenarioBattle b(
                    make_party(self_pet({kXingHe, kPaDa, kPaDa, kPaDa, kPaDa})),
                    make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 100)));
                b.start();
                BattleContext& ctx = b.ctx();
                sbs_srand(s);
                both_act(b, 0);
                if (ctx.getPet(1).hp <= 0) {
                    seed_kill = s;
                }
            }
            if (seed_drop < 0) {
                ScenarioBattle b(
                    make_party(self_pet({kXingHe, kPaDa, kPaDa, kPaDa, kPaDa})),
                    make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                sbs_srand(s);
                both_act(b, 0);
                if (ctx.getPet(1).hp > 0 && level_sum(ctx, 1) == -6) {
                    seed_drop = s;
                    drop_sum = level_sum(ctx, 1);
                }
            }
        }
        CHECK(seed_kill > 0, "T2383.a 1318：掷中 → 秒杀（自校准种子找到）");
        CHECK(seed_drop > 0, "T2383.b 1318：掷歪 → 全属性 -1（自校准种子找到）");
        CHECK_EQ(drop_sum, -6, "T2383.b 1318：六槽各 -1（含命中）");
        std::cout << "  [T2383] 1318 两半 通过 (seed " << seed_kill << "/" << seed_drop
                  << ")\n";
    }

    std::cout << "scenario_2380 PASS (J145 后手/兜底杂门)" << std::endl;
    return 0;
}
