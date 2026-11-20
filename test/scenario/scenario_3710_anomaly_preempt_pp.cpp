// 场景 3710 —— K227 异常先制+PP一降一升+全零追击族（2011，soul_lib/k227_anomaly_preempt_pp.cpp）
//（2026-10-08，day 专项 worktree ../agents/k227；号段 3710-3719 → T3710 一块四例）
//
// 断言口径（3071 先制翻盘同款 + 直改 pp 手法；两侧行参驱动数值）：
//   - a 先制门：R1 无异常 → 对手先手（负例）；自方铺烧伤后 R2 → 低速持印者
//     先制+2 翻盘先手（preemptive_right，3071 同锚）；
//   - b PP 一降一升（行1 [2,1,1,1]）：NONE 回合后对手全技能 -1、自身命中 db[2]=1
//     回复（自设 maxPP-2 → maxPP-1，封顶前）；
//   - c 全零追击：对手全技能 pp=1 → 一轮后全零挂旗 → 次回合用技 → 对手体力归零
//     （死亡后 FSM 停换宠位，getPet 仍为阵亡宠——2890.c 同款断法）；
//   - d 行2 [2,1,2,1]：db[2]=2 档回复（maxPP-3 → maxPP-1，行参驱动）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kMaxHp = 999999;

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(2011, "K227 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 4407;   // 赤虹仙凤
    return sm;
}

// 持印者：低速（先制翻盘可观测）、微攻（不误杀对手）。
ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType act0, BattleFsm::ActionType act1) {
    b.submit_operation(0, act0, act0 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.submit_operation(1, act1, act1 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3710.a 先制门（无异常负例 → 铺异常翻盘）══
    // ⚠ start() 停泊在 CHOOSE 时本回合 ROUND_START 尚未跑（K182 记档）——停泊点铺的
    // 异常会被"本轮" drive 途中的 ROUND_START 节点看见；负例必须放在铺异常**之前**。
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3710.a R1 全场无异常 → 高速对手先手（负例）");
        REQUIRE(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1));
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3710.a R2 异常在 → 低速持印者先制+2 翻盘先手");
        std::cout << "  [T3710.a] 先制门+翻盘 通过\n";
    }

    // ══ T3710.b PP 一降一升（行1 db[2]=1）══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_pp0 = ctx.getPet(1).skills[0].pp;
        const int foe_pp1 = ctx.getPet(1).skills[1].pp;
        ctx.getPet(0).skills[0].pp = ctx.getPet(0).skills[0].maxPP - 2;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_pp0 - 1, "T3710.b 对手技能0 PP-1");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, foe_pp1 - 1, "T3710.b 对手技能1 PP-1（全技能）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP - 1,
                 "T3710.b 自身技能0 +1 回复（maxPP-2 → maxPP-1）");
        std::cout << "  [T3710.b] PP 一降一升 通过\n";
    }

    // ══ T3710.c 全零追击：全零 → 下回合用技 → 对手体力归零 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = 1;
        }
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T3710.c 一轮后对手 PP 全零（旗挂下回合）");
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(1).hp, 0,
                 "T3710.c 追击回合用技 → 对手全部体力被消耗（atk=1 不可能打出 0 血，必为追击）");
        std::cout << "  [T3710.c] 全零追击 通过\n";
    }

    // ══ T3710.d 行2 [2,1,2,1]：db[2]=2 档回复 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].pp = ctx.getPet(0).skills[0].maxPP - 3;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP - 1,
                 "T3710.d 进化行 +2 回复（maxPP-3 → maxPP-1，行参驱动）");
        std::cout << "  [T3710.d] 行2 双倍回复 通过\n";
    }

    std::cout << "scenario_3710 PASS (K227 异常先制+PP+全零追击族)" << std::endl;
    return 0;
}
