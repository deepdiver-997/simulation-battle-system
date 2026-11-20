// 场景 480：E3 必暴族 —— effect 475（伤害不足→下N回合必暴）+ 1180（吸取成功→下N回合必暴）
//（moves_lib/e_must_crit.cpp，2026-09-29）
//
// 覆盖：
//   475  暗夜雷光击 14118（[300,2]）：伤害不足阈值 → 桥开 NextRounds 必暴窗；达标 → 不给
//   1180 破卵而出 31002（[2]）：吸取成功 → 下2回合必暴；无提升 → 不给；被 STAT_CLEAR
//        挡下 → 不给（"无法被消除或吸取"）
//   通道 = 58 圣光气同款（ROUND_START tick 重写 ws.must_crit；掷点 OR 读，crit_rate=0 也必暴）。
//   断言面 = crit_happened[owner]（query_usage ①.5 写入，行为级）+ 能力等级搬运结果。
//
// 确定性：必暴是强制的（无掷点）；暴击率被 harness 清零（负向断言可靠）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kThunder = 14118;   // 暗夜雷光击：475 [300,2]（特攻 150）
constexpr int kHit = 10004;       // 拍打：无副作用的普攻（窗口期验必暴用）
constexpr int kPoLuan = 31002;    // 破卵而出：1180 [2]

ElfPet attacker(std::array<int, 5> skills, int atk, int hp = 500) {
    ElfPet p = scenario::make_pet(0, "测试甲", skills, /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int hp = 1000000) {
    ElfPet p = scenario::make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/10, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 跑一个回合：双方各提交一次 op0/op1 指定技能，驱动到下一回合选择点。
void run_round(ScenarioBattle& b, int skill0, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 475 触发：伤害不足 300 → 下2回合必暴 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker({kThunder, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        run_round(b, 0, 0);   // R1：暗夜雷光击（atk=10 → 伤害远低于 300）
        CHECK(ctx.crit_happened[0] == false,
              "T1 前置：本回合未暴击（crit_rate=0，NextRounds 窗口当回合不覆盖）");

        run_round(b, 1, 0);   // R2：拍打（窗口期）
        CHECK(ctx.crit_happened[0] == true,
              "T1 核心：伤害不足 300 → 下1回合攻击必定致命一击");
        run_round(b, 1, 0);   // R3：拍打（窗口期第 2 回合）
        CHECK(ctx.crit_happened[0] == true, "T1 核心：窗口第 2 回合仍必暴（rounds=2）");
        run_round(b, 1, 0);   // R4：窗口已过
        CHECK(ctx.crit_happened[0] == false, "T1 核心：窗口过期恢复常规掷点");
        std::cout << "  [T1] 475 触发路径（不足300→下2回合必暴） 通过\n";
    }

    //========== T2 475 不触发：伤害达标 → 不给窗口 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker({kThunder, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        run_round(b, 0, 0);   // R1：暗夜雷光击（atk=10000、威力 150 → 伤害远超 300）
        CHECK(ctx.crit_happened[0] == false, "T2 前置：本回合未暴击");
        run_round(b, 1, 0);   // R2
        CHECK(ctx.crit_happened[0] == false,
              "T2 核心：伤害达标（≥300）→ 不开必暴窗");
        std::cout << "  [T2] 475 达标反例 通过\n";
    }

    //========== T3 1180 成功：吸取对手提升 → 下2回合必暴 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker({kPoLuan, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/2)
                == StatChangeResult::SUCCESS);   // 对手攻击 +2

        run_round(b, 0, 0);   // R1：破卵而出（吸取成功）
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3 核心：对手提升被吸走（清零）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T3 核心：自身获得等量提升");
        CHECK(ctx.crit_happened[0] == false, "T3 前置：本回合未暴击");

        run_round(b, 1, 0);   // R2
        CHECK(ctx.crit_happened[0] == true, "T3 核心：吸取成功 → 下1回合必暴");
        run_round(b, 1, 0);   // R3
        CHECK(ctx.crit_happened[0] == true, "T3 核心：窗口第 2 回合仍必暴（rounds=2）");
        run_round(b, 1, 0);   // R4
        CHECK(ctx.crit_happened[0] == false, "T3 核心：窗口过期恢复常规掷点");
        std::cout << "  [T3] 1180 吸取成功路径（含窗口时长） 通过\n";
    }

    //========== T4 1180 失败：对手无提升 → 不给窗口 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker({kPoLuan, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        run_round(b, 0, 0);   // R1：破卵而出（无提升可吸）
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T4 前置：自身无提升可得");
        run_round(b, 1, 0);   // R2
        CHECK(ctx.crit_happened[0] == false, "T4 核心：吸取失败（无可转化）→ 不开必暴窗");
        std::cout << "  [T4] 1180 无提升反例 通过\n";
    }

    //========== T5 1180 被挡：对手免消除强化（STAT_CLEAR）→ 吸取失败 → 不给窗口 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker({kPoLuan, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS);   // 对手攻击 +2
        ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_CLEAR, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/9, /*source_id=*/0,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/9999);

        run_round(b, 0, 0);   // R1：破卵而出（被挡）
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T5 核心：对手提升保留（免消除强化挡下吸取）");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T5 核心：自身未获得提升");
        run_round(b, 1, 0);   // R2
        CHECK(ctx.crit_happened[0] == false, "T5 核心：吸取被挡 → 不开必暴窗");
        std::cout << "  [T5] 1180 免消除强化反例 通过\n";
    }

    std::cout << "scenario_480 全部通过" << std::endl;
    return 0;
}
