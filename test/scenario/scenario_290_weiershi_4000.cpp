#include <utils/sbs_rand.h>
// 场景 290：薇尔诗 4000 五技 向未来的祈愿 38423（2500/1922/1880/2351；2075 跳过见实现头）
// （2026-09-28，night-3 / D13；实现 resources/moves_lib/d5_D13.cpp）
//
// T1 2500 最优系别：守方单暗影系（神灵中性 1.0、候选光克暗影 2.0）→ restraint_view=2.0。
// T2 1922：预挂对手回合类（弹控票）→ 消除成功 → 2 回合属性命中失效：R2 守方锋利
//     （属性技 攻+1）效果不落地；R3 窗口过期后照常 +1。
// T3 1880 吸取成功：守方预置 攻+2 → 被吸走（我 +2 / 守清零）→ R2 伤害窗口 +100%。
// T4 1880 无提升：R1 吸取 0 → R2 先制窗口 +2。
// T5 2351 未击败：守方存活 → 全属性 -1；有落地 → 不锁切。
// T6 2351 未触发：守方免弱（STAT_DROP 免疫）→ 6 槽全挡 → 锁切 2 回合。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;       // 拍打（物攻 40）
constexpr int kQiYuan = 38423;    // 向未来的祈愿
constexpr int kFengLi = 20265;    // 锋利：属性，自身攻击+1
constexpr int kElemDark = 13;     // 暗影

ElfPet weiershi() {
    ElfPet p = scenario::make_pet(4000, "薇尔诗", {kQiYuan, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int skill0 = kHit) {
    ElfPet p = scenario::make_pet(1, "守方", {skill0, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, int my_skill, int foe_skill) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 2500 最优系别：暗影守方 → 克制 2.0（神灵自然值 1.0） ==========
    {
        ElfPet foe = foe_pet();
        foe.elementalAttributes[0] = kElemDark;   // 单暗影 {13,0}
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe));
        b.start();
        one_round(b, 0, 0);
        CHECK_EQ(static_cast<int>(b.ctx().ws.restraint_view[0] * 100), 200,
                 "T1 五系最优：光克暗影 2.0 覆盖神灵中性 1.0");
        std::cout << "  [T1] 2500 最优系别通过\n";
    }

    //========== T2 1922 消回合成功 → 2 回合属性命中失效 ==========
    {
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe_pet(kFengLi)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);   // 对手回合类效果（可被消除）
        one_round(b, 0, 1);   // R1：32519 消回合成功，挂 2 回合命中失效；2351 亦全属性-1
        one_round(b, 1, 0);   // R2：守方锋利（属性技）→ 命中失效，攻+1 不落地
        CHECK_EQ(ctx.ability_levels[1][0], -1,
                 "T2 R2 命中失效：锋利攻+1 未落地（基线 -1 来自 2351 降属）");
        one_round(b, 1, 0);   // R3：窗口已过，锋利照常
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T2 R3 窗口过期：锋利攻+1 落地（-1→0）");
        std::cout << "  [T2] 1922 消回合→属性命中失效 通过\n";
    }

    //========== T3 1880 吸取成功：吸走提升 + R2 增伤窗口 ==========
    {
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 守方预置 攻+2（有提升可吸）
        one_round(b, 0, 0);   // R1：吸取（2351 亦全属性-1）
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T3 吸取成功：提升转移到我方");
        CHECK_EQ(ctx.ability_levels[1][0], -1,
                 "T3 吸取成功：守方提升被清（2351 再降 1 → -1）");
        one_round(b, 1, 0);   // R2：窗口（下1回合，ROUND_START tick +100%）
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 100,
                 "T3 R2 增伤窗口 +100%");
        one_round(b, 1, 0);   // R3：窗口已过
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0, "T3 R3 窗口已过");
        std::cout << "  [T3] 1880 吸取成功 → 下1回合增伤+100% 通过\n";
    }

    //========== T4 1880 对手无提升 → 下1回合先制+2 ==========
    {
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 0);   // R1：吸取 0 → 先制窗口武装
        one_round(b, 1, 0);   // R2：MOVE_RIGHT tick 先制+1（窗口第一回合）
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T4 R2 先制窗口 +1");
        one_round(b, 1, 0);   // R3：窗口第二回合仍 +1
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T4 R3 仍 +1");
        std::cout << "  [T4] 1880 无提升 → 下2回合先制+1 通过\n";
    }

    //========== T5 2351 未击败：全属性-1，不锁切 ==========
    {
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 0);   // R1：守方存活 → 全属性-1
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1, "T5 守方 6 槽各 -1");
        }
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount) == false,
              "T5 降属有落地：不锁切");
        std::cout << "  [T5] 2351 未击败 → 全属性-1 通过\n";
    }

    //========== T6 2351 未触发（全免弱）→ 锁切 2 回合 ==========
    {
        ScenarioBattle b(scenario::make_party(weiershi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/3, /*source_id=*/999);
        one_round(b, 0, 0);   // R1：降属全被免弱挡下
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], 0, "T6 免弱：守方等级未变");
        }
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "T6 未触发：锁切 2 回合");
        std::cout << "  [T6] 2351 未触发 → 锁切 通过\n";
    }

    std::cout << "scenario_290 all passed" << std::endl;
    return 0;
}
