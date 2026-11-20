#include <utils/sbs_rand.h>
// 场景 151：第一批共享 id 解锁的四只精灵（2026-09-27）
//
//   空元行者 4586     浩博 36996        [699]   —— 内核穿透凭证（effect_meta 路径）
//   光之惩戒·英卡洛斯 3577  斗气护体 26026 [620]  璨星断刃 31844 [699]
//   科穆宁 3405       石化之肤 25579 [94]   蛮横重锤 30938 [660]
//   不灭·艾恩斯 3467  星空火雨 11977 [111]  不灭意志 31270 [697]
//
// 699/697 的引擎口径 = loadSkills 把效果并入 Skills::penetration_flags（src/entities/
// skills.cpp penetration_flags_for_effect），出手时 materialize_attack_credential 消费。
// 场景断言技能对象上的凭证位（加载期一次性写入、确定性强）；穿透行为面由场景 004 覆盖。
// 石化之肤的 94 子句是 50% 概率——主路径（100% 版）已在场景 150 T9 锁死，这里只锁
// 该技能的确定性子句（585 全属性+1）。
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
constexpr int kBuff = 20265;   // 锋利：自身攻击+1（属性技）

// 载体宠：全技能必中（命中不掷骰）；速度 200 恒先手。
ElfPet carrier(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "载体", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp = 4000, int atk = 100) {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;   // 不置必中：给"必miss"类效果留语义空间
    }
    return p;
}

void use0(ScenarioBattle& b, int foe_slot = 0) {
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

    //========== T1 空元行者 4586 浩博：699 穿透凭证 ==========
    {
        ScenarioBattle b(scenario::make_party(carrier(36996)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[0].penetration_flags.ignore_attack_immunity,
              "T1 浩博 [699]：loadSkills 并入 ignore_attack_immunity 凭证");
        CHECK_EQ(ctx.getPet(0).skills[0].penetration_flags.level, 1, "T1 凭证等级 1（可穿盔）");
        std::cout << "  [T1] 空元行者 4586 浩博 699 通过（技能组应已全绿）\n";
    }

    //========== T2 英卡洛斯 3577 斗气护体：620 致命一击率 +5/16 ==========
    {
        ScenarioBattle b(scenario::make_party(carrier(26026)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        CHECK(std::abs(ctx.ws.crit_rate_add[0] - 5 * 100.0f / 16.0f) < 0.01f,
              "T2 斗气护体 [3,5]：crit_rate_add = 5/16（首手立即生效）");
        std::cout << "  [T2] 英卡洛斯 3577 斗气护体 620 通过\n";
    }

    //========== T3 英卡洛斯 3577 璨星断刃：699 凭证（攻击技能侧）==========
    {
        ScenarioBattle b(scenario::make_party(carrier(31844)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[0].penetration_flags.ignore_attack_immunity,
              "T3 璨星断刃 [699]：攻击技能携带穿透凭证");
        use0(b, /*foe_slot=*/1);
        CHECK(ctx.getPet(1).hp < 4000, "T3 璨星断刃主路径：攻击照常落地");
        std::cout << "  [T3] 英卡洛斯 3577 璨星断刃 699 通过\n";
    }

    //========== T4 科穆宁 3405 石化之肤：585 全属性+1（94 子句见场景 150 T9）==========
    {
        ScenarioBattle b(scenario::make_party(carrier(25579)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_level(0, i), 1, "T4 石化之肤：全属性+1（含命中槽）");
        }
        std::cout << "  [T4] 科穆宁 3405 石化之肤 通过\n";
    }

    //========== T5 科穆宁 3405 蛮横重锤：660[0] 消对手提升 → 回满 ==========
    {
        ScenarioBattle b(scenario::make_party(carrier(30938)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        ctx.getPet(0).hp = 2600;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.ctx().debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        b.ctx().debug_step_mode = false;
        BattleContext& c2 = b.ctx();
        CHECK_EQ(c2.ability_level(1, 0), 0, "T5 蛮横重锤：对手能力提升被消除");
        CHECK_EQ(c2.getPet(0).hp, 3000, "T5 消除成功：恢复 100% 体力");
        b.drive();
        std::cout << "  [T5] 科穆宁 3405 蛮横重锤 660 通过\n";
    }

    //========== T6 艾恩斯 3467 星空火雨：111 附加等级 scaled 固伤 ==========
    {
        // 引擎无等级维度 → 附加 200 定值（文件头记待拍板）。攻方 atk=10：红伤极小，
        // 守方总掉血应落在 [200, 320]（红伤 + 200 固伤 + 浮动带宽）。
        ScenarioBattle b(scenario::make_party(carrier(11977, /*atk=*/10)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/1);
        const int drop = 4000 - ctx.getPet(1).hp;
        CHECK(drop >= 200 && drop < 320,
              "T6 星空火雨：守方掉血 = 红伤(小) + 200 固伤，实际 " << drop);
        std::cout << "  [T6] 艾恩斯 3467 星空火雨 111 通过（drop=" << drop << "）\n";
    }

    //========== T7 艾恩斯 3467 不灭意志：697 无视伤害限制凭证 ==========
    {
        ScenarioBattle b(scenario::make_party(carrier(31270)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[0].penetration_flags.ignore_damage_limit,
              "T7 不灭意志 [697]：loadSkills 并入 ignore_damage_limit 凭证");
        std::cout << "  [T7] 艾恩斯 3467 不灭意志 697 通过（技能组应已全绿）\n";
    }

    std::cout << "scenario_151 all passed" << std::endl;
    return 0;
}
