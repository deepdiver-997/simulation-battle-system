// 场景 165：薇尔诗 4000 技能组缺口（一）—— 受击窗双件 + 低体免 PP
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果：
//   190 虚空之遗 22638：5 回合内若受到攻击 → 消除攻击方能力提升
//   406 禁忌魔光 22637：5 回合内受到攻击 100% 概率恢复 150 点体力
//   412 天辉圣印 32106：自身体力 < 1/2 → 每次使用不消耗 PP
//
// 确定性纪律：受击窗观察者对 0 伤命中同样触发（EVENT_TAKE_DAMAGE 红伤路径
// 无条件 emit），对照手 kHarmless（power 0）保证 hp 断言精确。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kXukong = 22638;   // 虚空之遗：190（属性、必中）
constexpr int kJinji = 22637;    // 禁忌魔光：406（属性、必中）
constexpr int kTianhui = 32106;  // 天辉圣印：412（物攻 140）
constexpr int kHit = 10004;      // 拍打（物攻 40）
constexpr int kHarmless = 10938; // 本能一击（power 0：0 伤命中仍发受击事件）

ElfPet wei(std::array<int, 5> skills, int speed = 200, int atk = 100, int hp = 100000) {
    ElfPet p = scenario::make_pet(0, "薇尔诗", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, /*hp=*/100000);
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

    //========== T1 190：受击 → 消除攻击方能力提升 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXukong, kHit, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手攻击+2（待清除）
        // R1：我武装 5 回合受击窗（先手）；对手 power 0 攻击命中我（0 伤受击事件）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T1 核心：受击 → 攻击方能力提升被消除");
        std::cout << "  [T1] 190 受击消提升 通过\n";
    }

    //========== T1b 190 对照：不受击不触发 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXukong, kHit, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kXukong, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;
        // R1：双方都用属性技 → 我未受击（无受击事件）→ 对手提升保留
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T1b 对照：不受击 → 消除不触发");
        std::cout << "  [T1b] 190 不受击对照 通过\n";
    }

    //========== T2 406：受击 → 100% 恢复 150 点体力 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kJinji, kHit, kHarmless, kHarmless,
                                                   kHarmless}, /*speed=*/200, /*atk=*/100,
                                                 /*hp=*/100000)),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300;   // 留出 150 回血空间（对手 power 0 → 0 伤命中）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp >= 449 && ctx.getPet(0).hp <= 450,
              "T2 核心：受击（power 0 最低伤 1）+ 100% 恢复 150 → 449~450");
        std::cout << "  [T2] 406 受击回血 通过\n";
    }

    //========== T3 412：体力 < 1/2 → 不消耗 PP；体力回满 → 恢复消耗 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kTianhui, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 5, "T3 前置：天辉圣印 maxPP=5");
        ctx.getPet(0).hp = 1000;   // < 1/2 → 免 PP
        // R1：我低体攻击（不耗 PP）；对手 power 0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 5,
                 "T3 核心：体力 < 1/2 → 本次使用不消耗 PP");
        CHECK(ctx.getPet(1).hp < 100000,
              "T3 前置：天辉圣印红伤照常落地（红伤带随机因子，只断落地不断 exact）");
        // R2：体力回满 → PP 正常消耗
        ctx.getPet(0).hp = 100000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 4, "T3 核心：体力回满 → PP 正常消耗");
        std::cout << "  [T3] 412 低体免 PP 通过\n";
    }

    std::cout << "scenario_165 all passed" << std::endl;
    return 0;
}
