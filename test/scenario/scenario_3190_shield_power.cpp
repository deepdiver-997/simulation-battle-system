#include <utils/sbs_rand.h>
// 场景 3190 —— K187 双方护盾梯度增伤族（2474，soul_lib/k187_shield_power.cpp）
//（2026-10-07，worktree ../agents/k187；号段 3190-3199 → T3190-T3192 三块）
//
// 断言口径（2861/733 同款**字段断言**）：debug_step 停泊到 FIRST_SKILL_EFFECT 跑完
// 读 ws.skill_power_view[0].value——威力视图已物化、本族节点已跑；不走伤害比例断言
// （威力改写重掷浮动不可对齐，SOP §五.1）。期望值 = 基线视图 ×(100+bonus)/100
// （与实现同一整数表达式，逐位相等）。基线视图 = 无印同技能停泊读数。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：物理攻击载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体

constexpr int kTestShieldSource = 990187000;

ElfPet mark_pet(int mark_id, const std::vector<int>& row, bool attr_skill = false) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    const int skill0 = attr_skill ? kSuoTou : kJi;
    ElfPet p = make_pet(1, "印主", {skill0, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(mark_id, "K187 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 停泊读印主攻击技威力视图（2861 范式）；shields = <own, opp> 预置护盾。
int probe_view(bool marked, const std::vector<int>& row, bool attr_skill,
               std::pair<int, int> shields = {0, 0}) {
    ScenarioBattle b(make_party(mark_pet(marked ? 2474 : 0, row, attr_skill)),
                     make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (shields.first > 0) {
        ctx.seerRobot[0].elfPets[0].shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                                     shields.first, kTestShieldSource,
                                                     /*duration=*/0);
    }
    if (shields.second > 0) {
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].shield_bank_.add(
            ShieldBank::kSoulShieldPriority, shields.second, kTestShieldSource,
            /*duration=*/0);
    }
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    b.drive();
    return view;
}

// 停泊读"视图是否被改写"（rewritten 旗；0 威力视图下乘法改写不可分辨，用旗判门）。
bool probe_rewritten(bool marked, const std::vector<int>& row, bool attr_skill,
                     std::pair<int, int> shields = {0, 0}) {
    ScenarioBattle b(make_party(mark_pet(marked ? 2474 : 0, row, attr_skill)),
                     make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (shields.first > 0) {
        ctx.seerRobot[0].elfPets[0].shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                                     shields.first, kTestShieldSource,
                                                     /*duration=*/0);
    }
    if (shields.second > 0) {
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].shield_bank_.add(
            ShieldBank::kSoulShieldPriority, shields.second, kTestShieldSource,
            /*duration=*/0);
    }
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const bool rewritten = ctx.ws.skill_power_view[0].rewritten;
    b.drive();
    return rewritten;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_75{5, 1, 75};
    const std::vector<int> row_100{5, 2, 100};
    const std::vector<int> row_empty{};

    // 基线视图（无印）：攻击技 35（撞击）；属性技威力恒 0（category=4 无伤害阶段）。
    const int base_atk = probe_view(false, row_empty, false);
    CHECK_EQ(base_atk, 35, "基线攻击技视图已物化（撞击威力 35）");

    // ══ T3190 [5,1,75] 梯度：双方护盾 150 点 → bonus=min(75, 150/5×1)=30 ══
    {
        const int v = probe_view(true, row_75, false, {100, 50});
        CHECK_EQ(v, base_atk * 130 / 100,
                 "T3190 2474：双方 150 盾 → 攻击伤害 +30%（视图逐位）");
        std::cout << "  [T3190] 梯度增伤 通过\n";
    }

    // ══ T3191 [5,1,75] 封顶：2000 盾 → 裸 400% → cap 75 ══
    {
        const int v = probe_view(true, row_75, false, {2000, 0});
        CHECK_EQ(v, base_atk * 175 / 100,
                 "T3191 2474：2000 盾 → 上限 +75%（cap 生效）");
        std::cout << "  [T3191] 上限封顶 通过\n";
    }

    // ══ T3192 进化行 [5,2,100]：100 盾 → 100/5×2=40（db 透传互锚）══
    {
        const int v = probe_view(true, row_100, false, {60, 40});
        CHECK_EQ(v, base_atk * 140 / 100,
                 "T3192 2474 进化行：双方 100 盾 → +40%（db[1]=2 透传）");
        std::cout << "  [T3192] 进化行互锚 通过\n";
    }

    // ══ T3193 门对照（rewritten 旗判，0 威力视图不可分辨乘法改写）：══
    {
        CHECK(!probe_rewritten(true, row_75, true, {100, 50}),
              "T3193 2474：属性技 → 节点未写视图（类型门）");
        CHECK(!probe_rewritten(true, row_75, false, {0, 0}),
              "T3193 2474：零盾 → 节点未写视图（bonus=0 早退）");
        CHECK(probe_rewritten(true, row_75, false, {100, 50}),
              "T3193 2474：有盾攻击技 → rewritten 置位（正向对照）");
        std::cout << "  [T3193] 属性技/零盾门对照 通过\n";
    }

    std::cout << "scenario_3190 PASS (K187 双方护盾梯度增伤族)" << std::endl;
    return 0;
}
