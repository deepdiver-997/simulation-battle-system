#include <utils/sbs_rand.h>
// 场景 152：王之哈莫 3022 技能组（2026-09-27）
//
//   龙腾寰宇 19227 [880]   消除对手回合类效果，消除成功则自身下回合先制+3
//   万里龙吟 24706 [440]   3回合内对手使用技能消耗的PP值变为2倍（+444 已实现）
//   千秋龙威 24707 [884]   令对手下次使用的技能无效（包括必中）
//
// T1 880：先给对手挂回合类效果（弹控票）→ 用龙腾寰宇 → 断回合成功 → R2 的
//     MOVE_RIGHT tick 写 ws.preemptive_level[0] += 3（NextRounds 窗口，867 同款落点）。
// T2 440：用万里龙吟 → 本回合立即写倍数（对手 AFTER_ACTION 消耗 PP 时按 2 扣）。
// T3 884：用千秋龙威 → 对手身上有 count=1 的 SEAL_ALL 盔 → R2 对手出手被无效。
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

constexpr int kHit = 10004;     // 拍打（物攻 40）
constexpr int kBuff = 20265;    // 锋利（自身攻击+1，属性技）
constexpr int kLongTeng = 19227;   // 龙腾寰宇
constexpr int kWanLi = 24706;      // 万里龙吟
constexpr int kQianQiu = 24707;    // 千秋龙威

ElfPet hamo(int skill) {
    ElfPet p = scenario::make_pet(0, "王之哈莫", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, 300, 3000);
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

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 龙腾寰宇 880：消回合成功 → 自身下回合先制+3 ==========
    // 引擎口径注：选择期基值先制条目按技能 id 去重、跨回合累积（R1 用过的龙腾寰宇
    // 先制+3 在 R2 的 MOVE_RIGHT 桶仍会 tick）——为隔离被测面，T1 用"对照组差值"断言：
    // 对照（不挂回合类效果 → 消除失败 → 无 880 窗口）与实验（挂 → 消除成功 → 窗口）
    // 的 R2 先制差值应恰为 +3。
    int control_lvl = -1;
    {
        ScenarioBattle b(scenario::make_party(hamo(kLongTeng)),
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
        control_lvl = ctx.ws.preemptive_level[0];
        b.drive();
        std::cout << "  [T1c] 对照组 R2 先制（无 880 窗口）= " << control_lvl << "\n";
    }
    {
        ScenarioBattle b(scenario::make_party(hamo(kLongTeng)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_reflect(1, /*source_slot=*/-1, /*source_effect_id=*/999,
                                       /*target=*/1, /*rounds=*/3);
        CHECK(ctx.rule_center_.has_round_type(1), "T1 前提：对手处于回合类效果");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(!ctx.rule_center_.has_round_type(1), "T1 对手回合类效果被消除");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        const int got = ctx.ws.preemptive_level[0];
        CHECK_EQ(got - control_lvl, 3, "T1 880 窗口：下回合自身先制+3（差值隔离）");
        b.drive();
        std::cout << "  [T1] 龙腾寰宇 880 通过（对照 " << control_lvl << " → 实验 " << got
                  << "）\n";
    }

    //========== T2 万里龙吟 440：对手 PP 消耗变 2 倍 ==========
    {
        ScenarioBattle b(scenario::make_party(hamo(kWanLi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp_before = ctx.getPet(1).skills[1].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_pp_cost_multiplier[1], 2, "T2 倍数已写入（本回合立即生效）");
        // 440 倍数扣 2 + 万里龙吟另一子句 444"降低对手PP1点" → 该槽共 -3
        CHECK_EQ(ctx.getPet(1).skills[1].pp, pp_before - 3,
                 "T2 对手本次消耗 2(倍数)+1(444) 点 PP");
        std::cout << "  [T2] 万里龙吟 440 通过\n";
    }

    //========== T3 千秋龙威 884：对手下次使用的技能无效 ==========
    // 攻方先手用千秋龙威（属性技）→ 授 count=1 SEAL_ALL → **同一回合**对手（后手）的
    // 出手即"下次使用" → 被无效；R2 次数已耗 → 对手攻击照常落地（一次性语义）。
    {
        ScenarioBattle b(scenario::make_party(hamo(kQianQiu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 3000,
                 "T3 千秋龙威：对手本次出手被无效（无红伤落地）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.getPet(0).hp < 3000, "T3 次数型：R2 对手攻击照常（盔已消耗）");
        std::cout << "  [T3] 千秋龙威 884 通过\n";
    }

    std::cout << "scenario_152 all passed" << std::endl;
    return 0;
}
