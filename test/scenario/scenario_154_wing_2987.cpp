#include <utils/sbs_rand.h>
// 场景 154：重生之翼 2987 技能组（2026-09-27）
//
//   银雾之翼 19100 [867+868+866]  消回合+消提升成功 → 对手下回合先制-2 各一次
//   挚金命轮 24638 [749+861+756]  4回合内对手用属性技 → 全技能PP-2；命中后70%失明
//   纯光结界 24639 [128+456]      下回合受到的伤害转化为恢复自身体力
//   无上天命剑 19102 [699+864+865+807]  未击败→下回合反弹1/2；附加对手上次伤害固伤
//
// 银雾之翼的 866（暴击神耀能量）与 756（70% 失明）为概率/魂印联动子句：
// 866 依赖魂印侧计数器（wing_skills.cpp 已实现），此处只锁技能侧确定性子句；
// 756 概率分支不精确断言（100% 版语义由 94/1138 族覆盖）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;
constexpr int kBuff = 20265;
constexpr int kYinWu = 19100;    // 银雾之翼
constexpr int kZhiJin = 24638;   // 挚金命轮
constexpr int kChunGuang = 24639;// 纯光结界
constexpr int kMingJian = 19102; // 无上天命剑

ElfPet wing(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "重生之翼", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, 100, 4000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void use0(ScenarioBattle& b, int foe_slot = 1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 银雾之翼：867 消回合 + 868 消提升 → 对手下回合先制 -4 ==========
    {
        // 对照：不挂回合类/提升 → 两个消除都失败 → 无先制窗口
        int control = -1;
        {
            ScenarioBattle b(scenario::make_party(wing(kYinWu)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
            ctx.debug_step_mode = false;
            control = ctx.ws.preemptive_level[1];
            b.drive();
        }
        // 实验：对手有回合类效果 + 能力提升 → 双消除成功 → -2 + -2 = -4
        ScenarioBattle b(scenario::make_party(wing(kYinWu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(!ctx.rule_center_.has_round_type(1), "T1 867：对手回合类效果被消除");
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T1 868：对手能力提升被消除");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        const int got = ctx.ws.preemptive_level[1];
        CHECK_EQ(got - control, -4, "T1 双窗口：下回合对手先制 -2-2=-4（差值隔离）");
        b.drive();
        std::cout << "  [T1] 银雾之翼 867+868 通过（对照 " << control << " → 实验 " << got
                  << "）\n";
    }

    //========== T2 挚金命轮 861：窗口内对手用属性技 → 所有技能 PP-2 ==========
    {
        ScenarioBattle b(scenario::make_party(wing(kZhiJin)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/0);   // R1 挂 4 回合窗口；守方锋利（不消耗攻击 PP）
        const int used = ctx.getPet(1).skills[0].pp;    // 锋利槽
        const int other = ctx.getPet(1).skills[1].pp;   // 拍打槽
        // R2 守方用锋利（属性技）→ 861 tick：所有技能 -2；锋利槽再消耗 1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[1].pp, other - 2,
                 "T2 未使用槽：对手所有技能 PP -2（861）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, used - 3,
                 "T2 使用槽：PP -2（861）再 -1（自身消耗）");
        std::cout << "  [T2] 挚金命轮 861 通过\n";
    }

    //========== T3 纯光结界 128：下回合受到的伤害转化为自身体力 ==========
    {
        ScenarioBattle b(scenario::make_party(wing(kChunGuang)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2600;   // 预留净增空间
        use0(b, /*foe_slot=*/1);   // R1 挂 1 回合转化窗口；守方拍打打过来
        const int taken = ctx.ws.raw_attack_damage[1];
        CHECK(taken > 0, "T3 前提：守方打出攻击伤害");
        CHECK_EQ(ctx.getPet(0).hp, 2600 + taken, "T3 伤害转化为体力（净增口径）");
        std::cout << "  [T3] 纯光结界 128 通过（taken=" << taken << "）\n";
    }

    //========== T4 无上天命剑 807：附加对手上次造成伤害数值的固定伤害 ==========
    {
        ScenarioBattle b(scenario::make_party(wing(kMingJian)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/1);   // R1：无上天命剑（台账刚建，无历史 → 不附加）；守方拍打
        const int foe_dealt = 3000 - ctx.getPet(0).hp;   // 守方 R1 打出的伤害 → 入台账
        CHECK(foe_dealt > 0, "T4 前提：守方 R1 造成过伤害");
        const int foe_hp_after_r1 = ctx.getPet(1).hp;
        // R2：再用无上天命剑 → 附加"对手上次伤害"固伤 + 红伤
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int drop_r2 = foe_hp_after_r1 - ctx.getPet(1).hp;
        CHECK(drop_r2 > foe_dealt,
              "T4 R2 掉血 = 红伤 + 对手上次伤害固伤（" << drop_r2 << " > " << foe_dealt
              << "）");
        std::cout << "  [T4] 无上天命剑 807 通过（foe_dealt=" << foe_dealt
                  << " drop_r2=" << drop_r2 << "）\n";
    }

    //========== T5 无上天命剑 864：未击败 → 下回合反弹受到伤害的 1/2 ==========
    {
        // R1 用无上天命剑（守方残血不致死 → 未击败）→ R2 反弹窗口 → 守方拍打打过来
        // → 守方被弹落伤的 1/2。R1 同时 807 会附加固伤（上一测试已锁），这里看反弹增量。
        // 攻方自带一个属性技（kBuff）作 R2 的"不攻击"动作，隔离反弹观测
        ElfPet attacker = wing(kMingJian);
        attacker.skills[1] = Skills(kBuff);
        ScenarioBattle b(scenario::make_party(attacker),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 1500;   // 守方残血（不致死：atk=300 power150 → ~500）
        use0(b, /*foe_slot=*/1);   // R1：未击败 → 864 arm；守方拍打（台账入账）
        const int taken_r1 = 3000 - ctx.getPet(0).hp;
        CHECK(taken_r1 > 0, "T5 前提：守方 R1 造成过伤害");
        CHECK(ctx.getPet(1).hp > 0, "T5 前提：未击败对手");
        const int foe_hp = ctx.getPet(1).hp;
        // R2：攻方用属性技（不出红伤）；守方拍打攻击 → 落伤 taken_r2 → 被弹 1/2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int taken_r2 = 3000 - ctx.getPet(0).hp - taken_r1;
        CHECK(taken_r2 > 0, "T5 前提：R2 守方攻击落地");
        const int reflected = foe_hp - ctx.getPet(1).hp;   // 守方 R2 总掉血（无红伤来源）
        CHECK_EQ(reflected, taken_r2 / 2, "T5 反弹受到伤害的 1/2");
        std::cout << "  [T5] 无上天命剑 864 通过（taken_r2=" << taken_r2
                  << " reflected=" << reflected << "）\n";
    }

    std::cout << "scenario_154 all passed" << std::endl;
    return 0;
}
