// 场景 180：重生之翼 2987 第五技「正义天启歌 19103」缺失效果 863
//（moves_lib/d5_D2.cpp，夜跑 night-2，2026-09-28）
//
// 863 官方模板："后出手则下{0}回合令对手使用的攻击技能无效"（携带切片 [1]）；
// 4399 图鉴："…后出手则下回合免疫受到的攻击"。
// 实现 = SKILL_EFFECT 判后手（acting_side_is_first）→ SEAL_ATTACK 票 rounds+1 +
// NextRounds condition 门（1055 定版范式；直接授 rounds 会在当回合末被 tick 清空）。
//
// 断言组：
//   T1 主路径：后手用正义天启歌 → 下回合对手攻击 SKILL_INVALID、伤害 0；
//              我方攻击照常落地（只封对手）。
//   T2 对照：先手用正义天启歌 → 不武装，对手攻击正常（非 INVALID、伤害 > 0）。
//   T3 窗口：只封下 1 回合——第 3 回合对手攻击恢复（我方第 2/3 回合改拍打不重挂）。
//   T4 类别：被封回合对手用属性技（高速移动）不受封。
//
// 确定性：全场 must_hit=1、accuracy=100；19103 携带的 862 在无魂印 866 时空转
//（wing_host 判 soulMark.id==866）、697 无伤害限制可穿即无行为；无概率掷点。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kTianqi = 19103;  // 正义天启歌：王系物攻 170，effect [862, 697, 863]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果
constexpr int kGaoSu = 20017;   // 高速移动：属性技、必中、无概率（速度+2）

ElfPet wing(int speed, std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "重生之翼", skills, speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 主路径：后手用正义天启歌 → 下回合对手攻击无效 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wing(/*speed=*/50, {kTianqi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(
                             foe(/*speed=*/100, {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 回合1：对手先手拍打；重生之翼后手正义天启歌 → 武装"下1回合封攻击"。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp > 0 && ctx.getPet(1).hp > 0, "T1 前提：双方存活");
        CHECK(ctx.ws.raw_attack_damage[0] > 0, "T1 前提：正义天启歌本体伤害照常落地");
        // 回合2：对手攻击 → 被封；我方拍打照常。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T1 核心：下回合对手攻击技能无效");
        CHECK_EQ(ctx.ws.raw_attack_damage[1], 0, "T1 核心：被封攻击没有伤害");
        CHECK(ctx.ws.raw_attack_damage[0] > 0, "T1 只封对手：我方攻击照常");
        std::cout << "  [T1] 后手 → 下回合封攻击 通过\n";
    }

    //========== T2 对照：先手用正义天启歌 → 不满足"后出手"，不武装 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wing(/*speed=*/200, {kTianqi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(
                             foe(/*speed=*/100, {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 回合1：重生之翼先手正义天启歌；对手后手拍打 → 应照常生效。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T2 核心：先手不武装，对手攻击正常生效");
        CHECK(ctx.ws.raw_attack_damage[1] > 0, "T2 对手伤害落地");
        std::cout << "  [T2] 先手不武装对照 通过\n";
    }

    //========== T3 窗口：只封下 1 回合；第 3 回合对手攻击恢复 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wing(/*speed=*/50, {kTianqi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(
                             foe(/*speed=*/100, {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 回合1：武装。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 回合2：被封（我方改拍打，不重挂）。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T3 前提：下回合对手攻击被封");
        // 回合3：窗口已过 → 对手攻击恢复。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T3 核心：第 3 回合对手攻击不再被封（窗口只 1 回合）");
        CHECK(ctx.ws.raw_attack_damage[1] > 0, "T3 对手伤害恢复落地");
        std::cout << "  [T3] 窗口 1 回合过期 通过\n";
    }

    //========== T4 类别：被封回合对手用属性技不受封 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wing(/*speed=*/50, {kTianqi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(
                             foe(/*speed=*/100, {kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 回合1：对手拍打；翼正义天启歌 → 武装。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 回合2：对手用属性技（高速移动）→ 只封攻击，属性技不受封。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T4 核心：封攻击不波及属性技（高速移动正常生效）");
        std::cout << "  [T4] 属性技不受封 通过\n";
    }

    std::cout << "scenario_180 all passed" << std::endl;
    return 0;
}
