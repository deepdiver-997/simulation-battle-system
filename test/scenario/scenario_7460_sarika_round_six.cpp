// 场景 7460 —— K393 犀牛（不灭地威·萨瑞卡 1217）补第三条：回合开始六项能力变化
//
// 官方原文（effect_icon row 928 / row 1474 两行变体，effect_id=1217，pet 3637）：
//   「回合开始时令自身防御+1、特防+1、速度+1，令对手防御-1、特防-1、速度-1；|自身受到的
//     攻击伤害大于350则恢复自身全部体力，……（boss无效）」
// 实现落点：legacy_guards.cpp SOUL_PROGRAM(1217) 补 BATTLE_ROUND_START 节点
//（自身 防/特防/速 stat_change +1；对手三槽 stat_drop 原语，actor=施术者）。
//
// 断言块：
//   A 回合 1 后双方六槽精确值（own +1 三槽 / opp -1 三槽，攻/特攻/命中槽恒 0）
//   B 连续两回合叠层（±2）
//   C ±6 钳位边界（R6 到顶，R7 不再变）
//   D 对手免弱票（STAT_DROP）存在 → 降档被挡、自身增益照常
//   E 换宠语义实测定案（STAGE scope）：持有者换下 → 六项全停 + 场上等级随换宠清零；
//     对手换宠 → 等级清零；持有者换回当回合即恢复施加——换入在回合开始处理、
//     **先于 ROUND_START 桶**（K393 接管轮实测：R4 换回 +1、R5 叠到 +2；
//     分两步断言同时防"重入场双重注册"——若 R5 一步跳 +2 而 R4 为 0 即双重开火）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;         // 高速移动：属性技（无伤害）
constexpr int kSarikaMarkId = 1217;
constexpr int kDef = 2, kSpDef = 3, kSpeed = 4;   // 引擎等级槽序（0攻1特攻2防3特防4速5命中）

ElfPet make_sarika() {
    // 速度 100 vs 对手 90：即使 +1/-1 等级（150 vs 60）也不翻转出手权（本场景全属性技，无碍）。
    return scenario::make_pet(0, "不灭地威·萨瑞卡", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              SoulMark(kSarikaMarkId, "不灭地威·萨瑞卡", "犀牛"));
}

ElfPet make_foe() {
    return scenario::make_pet(1, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/90, /*atk=*/10, /*hp=*/1000000);
}

// 跑一回合（双方 NONE——回合类照常触发、无伤害干扰）。
void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
}

void check_three(BattleContext& ctx, int side, int def, int spdef, int speed,
                 const char* tag) {
    CHECK_EQ(ctx.ability_level(side, kDef), def, tag);
    CHECK_EQ(ctx.ability_level(side, kSpDef), spdef, tag);
    CHECK_EQ(ctx.ability_level(side, kSpeed), speed, tag);
}

void check_untouched(BattleContext& ctx, int side, const char* tag) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == kDef || slot == kSpDef || slot == kSpeed) {
            continue;
        }
        CHECK_EQ(ctx.ability_level(side, slot), 0, tag);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A 回合 1：六槽精确值 ==========
    {
        ScenarioBattle b(scenario::make_party(make_sarika()),
                         scenario::make_party(make_foe()));
        BattleContext& ctx = b.ctx();
        b.start();
        idle_round(b);
        check_three(ctx, 0, 1, 1, 1, "A 自身 防+1/特防+1/速+1");
        check_three(ctx, 1, -1, -1, -1, "A 对手 防-1/特防-1/速-1");
        check_untouched(ctx, 0, "A 自身攻/特攻/命中槽不动");
        check_untouched(ctx, 1, "A 对手攻/特攻/命中槽不动");
        std::cout << "  [A] R1 六项精确值通过\n";
    }

    //========== B 连续两回合叠层 ==========
    {
        ScenarioBattle b(scenario::make_party(make_sarika()),
                         scenario::make_party(make_foe()));
        BattleContext& ctx = b.ctx();
        b.start();
        idle_round(b);
        idle_round(b);
        check_three(ctx, 0, 2, 2, 2, "B 自身两回合叠到 +2");
        check_three(ctx, 1, -2, -2, -2, "B 对手两回合叠到 -2");
        std::cout << "  [B] R2 叠层 ±2 通过\n";
    }

    //========== C ±6 钳位边界 ==========
    {
        ScenarioBattle b(scenario::make_party(make_sarika()),
                         scenario::make_party(make_foe()));
        BattleContext& ctx = b.ctx();
        b.start();
        for (int r = 0; r < 6; ++r) {
            idle_round(b);
        }
        check_three(ctx, 0, 6, 6, 6, "C R6 自身三槽到 +6 顶");
        check_three(ctx, 1, -6, -6, -6, "C R6 对手三槽到 -6 底");
        idle_round(b);   // R7：钳位外不再变
        check_three(ctx, 0, 6, 6, 6, "C R7 自身钳在 +6（不溢出）");
        check_three(ctx, 1, -6, -6, -6, "C R7 对手钳在 -6（不穿底）");
        std::cout << "  [C] ±6 钳位边界通过\n";
    }

    //========== D 对手免弱票 → 降档被挡、自身增益照常 ==========
    {
        ScenarioBattle b(scenario::make_party(make_sarika()),
                         scenario::make_party(make_foe()));
        BattleContext& ctx = b.ctx();
        b.start();
        // 手动挂 STAT_DROP 免疫票（场景 015 直调先例；9 回合窗口覆盖本块）
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/9997,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/9997);
        idle_round(b);
        check_three(ctx, 0, 1, 1, 1, "D 免弱只挡降档：自身增益照常 +1");
        check_three(ctx, 1, 0, 0, 0, "D 对手三槽降档全被免弱票挡下");
        std::cout << "  [D] 对手免弱挡降档通过\n";
    }

    //========== E 换宠语义（STAGE scope 实测定案）==========
    {
        auto party0 = scenario::make_party(make_sarika());
        party0[1] = scenario::make_pet(0, "替宠A", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
        auto party1 = scenario::make_party(make_foe());
        party1[1] = scenario::make_pet(1, "替宠B", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/90, /*atk=*/10, /*hp=*/1000000);
        ScenarioBattle b(party0, party1);
        BattleContext& ctx = b.ctx();
        b.start();
        idle_round(b);   // R1 基线：own +1×3、opp -1×3
        check_three(ctx, 0, 1, 1, 1, "E R1 基线 自身+1");
        check_three(ctx, 1, -1, -1, -1, "E R1 基线 对手-1");
        // R2 双方换宠：等级本体随换宠清零（on-stage 作用域）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        check_three(ctx, 0, 0, 0, 0, "E R2 换宠后自身（替宠A）等级清零");
        check_three(ctx, 1, 0, 0, 0, "E R2 对手（替宠B）等级清零");
        // R3：萨瑞卡在场下 → 1217 节点（STAGE scope）不跑 → 双方都不再变化
        idle_round(b);
        check_three(ctx, 0, 0, 0, 0, "E R3 持有者不在场：替宠A 不自升");
        check_three(ctx, 1, 0, 0, 0, "E R3 持有者不在场：替宠B 不被降");
        // R4 萨瑞卡回场：换入在回合开始处理、先于 ROUND_START 桶 → 当回合即 +1/-1
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        check_three(ctx, 0, 1, 1, 1, "E R4 换回当回合：自身已重新 +1（换入先于回合开始桶）");
        check_three(ctx, 1, -1, -1, -1, "E R4 换回当回合：对手已重新 -1");
        idle_round(b);   // R5：连续施加叠层
        check_three(ctx, 0, 2, 2, 2, "E R5 持有者回场后：自身叠到 +2");
        check_three(ctx, 1, -2, -2, -2, "E R5 持有者回场后：对手叠到 -2");
        std::cout << "  [E] 换宠失活/回场恢复通过\n";
    }

    std::cout << "scenario_7460 all passed" << std::endl;
    return 0;
}
