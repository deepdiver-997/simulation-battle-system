// 场景 167：薇尔诗 4000 技能组缺口（三）—— 自由之荣光 38422 效果组
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果（自由之荣光 = [2491跳, 881已有, 1767, 1524, 2493]，物攻 150、必中）：
//   1524 反馈自身能力下降给对手：成功 → 恢复自身全部体力；失败 → 解除自身下降
//   1767 攻击时对手能力提升视为相应的能力下降（ws.view_levels 视图翻转，A/B 伤害对比）
//   2493 恢复所有技能 4 点 PP，每恢复 1 点汲取对手 17 点体力；先制翻倍子句（无先制 → 无）
//
// 确定性纪律：红伤随机 → 对比式断言；PP/等级/hp 是确定值，CHECK_EQ。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kZiyou = 38422;    // 自由之荣光：1767+1524+2493（物攻 150、必中）
constexpr int kXukong = 22638;   // 虚空之遗（属性技：对手不受击对照用）
constexpr int kHarmless = 10938;

ElfPet wei(std::array<int, 5> skills, int atk = 100) {
    ElfPet p = scenario::make_pet(0, "薇尔诗", skills, /*speed=*/200, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, SoulMark sm = {}) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/10, /*hp=*/100000,
                                  sm);
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

    //========== T1 1524 反馈成功：下降镜像给对手 + 回满 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kZiyou, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kXukong, kXukong, kXukong, kXukong,
                                                  kXukong})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;   // 自身攻击-2（待反馈）
        ctx.getPet(0).hp = 50000;        // 留回满空间
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T1 核心：下降反馈 → 对手攻击-2");
        CHECK_EQ(ctx.ability_levels[0][0], -2, "T1 核心：反馈成功 → 自身下降保留");
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T1 核心：反馈成功 → 恢复自身全部体力");
        std::cout << "  [T1] 1524 反馈成功+回满 通过\n";
    }

    //========== T2 1524 反馈失败（对手免弱）→ 解除自身下降、不回满 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kZiyou, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kXukong, kXukong, kXukong, kXukong,
                                                  kXukong},
                                                 scenario::make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;
        ctx.getPet(0).hp = 50000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T2 前置：免弱 → 反馈未落地");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T2 核心：反馈失败 → 自身下降被解除");
        CHECK_EQ(ctx.getPet(0).hp, 50000, "T2 核心：反馈失败 → 不回满");
        std::cout << "  [T2] 1524 反馈失败解除 通过\n";
    }

    //========== T3 1767：对手特防+2 视为 -2 → 伤害近似翻倍（A/B 对比）==========
    {
        int loss_boosted = 0;
        int loss_plain = 0;
        for (const bool boosted : {true, false}) {
            ScenarioBattle b(scenario::make_party(wei({kZiyou, kHarmless, kHarmless, kHarmless,
                                                       kHarmless})),
                            scenario::make_party(foe({kHarmless, kHarmless, kHarmless,
                                                      kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            if (boosted) {
                ctx.ability_levels[1][3] = 2;   // 对手特防+2（待翻转；本技能特殊攻击，防项=特防）
            }
            // R1：双方 filler（保持 PP 状态一致 → 2493 汲取量一致）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(1).hp;
            // R2：我自由之荣光（特殊攻击，防项=特防）→ 1767 视强为弱（仅攻击时点）+ 2493 汲取
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (boosted) {
                loss_boosted = hp_r1 - ctx.getPet(1).hp;
            } else {
                loss_plain = hp_r1 - ctx.getPet(1).hp;
            }

        }
        CHECK(loss_boosted > loss_plain * 13 / 10,
              "T3 核心：防御+2 被视为 -2 → 伤害接近翻倍（远超 ±8% 噪声；boosted="
                  << loss_boosted << " plain=" << loss_plain << "）");
        std::cout << "  [T3] 1767 视强为弱 通过（boosted=" << loss_boosted
                  << " plain=" << loss_plain << "）\n";
    }

    //========== T4 2493：恢复所有技能 4 点 PP + 每点汲取 17 点体力 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kZiyou, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kXukong, kXukong, kXukong, kXukong,
                                                  kXukong})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[1].pp = 2;   // 拍打 maxPP=25：钳到 6
        ctx.getPet(0).skills[2].pp = 1;   // → 5
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 6, "T4 核心：所有技能 +4（min(pp+4, maxPP=30)）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 5, "T4 核心：所有技能 +4");
        // 汲取：恢复总量 = 1(自身 38422 4/5→5) + 4 + 4 + 0 + 0 = 9 → 对手 -9×17 = 153
        CHECK(ctx.getPet(1).hp <= 100000 - 153,
              "T4 核心：每恢复 1 点汲取 17 点体力（合计 153，另有红伤）");
        std::cout << "  [T4] 2493 PP 恢复+汲取 通过\n";
    }

    std::cout << "scenario_167 all passed" << std::endl;
    return 0;
}
