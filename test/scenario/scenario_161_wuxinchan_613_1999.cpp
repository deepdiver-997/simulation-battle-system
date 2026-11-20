// 场景 161：武心婵 4500 技能组缺口（二）—— 梦启/梦示/梦录的 613 + 1999
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
//   613  {0}回合内自身令对手使用的{1}系攻击技能无效（梦启=王系221/梦示=轮回系224/梦录=神灵系223）
//   1999 {0}回合内自身使用的攻击技能对{1}系精灵无效（梦启=火系3/梦示=草系1/梦录=水系2）
//
// 实现 = ATTACK_NULLIFY 票动态管理（1090 既有机制）：613 按"对手本回合选中的技能"评、
// 1999 按"对手在场精灵系别视图"评；MOVE_RIGHT 每回合评一次 + 1999 另挂
// EVENT_ENTER_STAGE（对手换宠即时重评，顺序敏感）。
//
// 断言组：T1 613 窗口内王系攻击被无效（5 回合逐回合）+ 窗口外恢复 /
//         T2 613 对照：非王系攻击不受影响 / T3 1999 对水系精灵攻击无效 + 对照 /
//         T4 1999 顺序敏感：对手换上水系 → 即刻补票（不用等下一回合）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kMengQi = 36629;    // 梦启：613[5,221] + 1999[5,3]（物攻 50）
constexpr int kMengLu = 36631;    // 梦录：613[5,223] + 1999[5,2]（物攻 50）
constexpr int kWangHit = 19101;   // 剑破苍穹：王系(221)物攻 130、先制 0（对手用；
                                  // 银雾之翼 19100 先制+3 会让对手先手、武装前就打死测试宠）
constexpr int kHit = 10004;       // 拍打（非王系对照）
constexpr int kHarmless = 10938;  // 本能一击

ElfPet wuxin(std::array<int, 5> skills, int speed = 200) {
    ElfPet p = scenario::make_pet(0, "武心婵", skills, speed, /*atk=*/100, /*hp=*/500);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        if (s.id == kMengQi || s.id == kMengLu) {
            s.power = 0;   // 主伤钳 1：排除梦技自身伤害对血量断言的干扰
        }
    }
    return p;
}

// 对手：可控元素 + 可控技能。
ElfPet foe(std::array<int, 5> skills, std::array<int, 2> elem = {0, 0}, int atk = 1000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, /*hp=*/50000);
    p.elementalAttributes = elem;
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

    //========== T1 613：5 回合内对手王系攻击无效；第 6 回合窗口过期恢复 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wuxin({kMengQi, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kWangHit, kHarmless, kHarmless, kHarmless,
                                                   kHarmless}, /*elem=*/{0, 0}, /*atk=*/40)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 第 1 回合：梦启武装（对手本回合选中王系攻击 → 即刻补票）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 500,
                 "T1 核心：613 武装当回合，对手王系攻击已被无效（红伤归零）");
        // 第 2~5 回合：窗口内 → 王系攻击继续无效
        for (int round = 2; round <= 5; ++round) {
            for (Skills& sk : ctx.getPet(1).skills) {
                sk.pp = sk.maxPP;   // 剑破苍穹 max_pp=5：手动回满防第 6 回合 PP 见底
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 500,
                     "T1 窗口内：第 " + std::to_string(round) + " 回合王系攻击仍无效");
        }
        // 第 6 回合：InRounds(5) 窗口已过（rc=5 ≥ start0+5）→ 攻击恢复
        for (Skills& sk : ctx.getPet(1).skills) {
            sk.pp = sk.maxPP;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < 500, "T1 核心：第 6 回合窗口过期 → 王系攻击恢复生效");
        std::cout << "  [T1] 613 王系攻击无效 5 回合 + 过期恢复 通过\n";
    }

    //========== T2 613 对照：对手用非王系攻击 → 不受影响 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             wuxin({kMengQi, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kHit, kHarmless, kHarmless, kHarmless,
                                                   kHarmless}, /*elem=*/{0, 0}, /*atk=*/40)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < 500,
              "T2 核心：对手用拍打（非王系）→ 613 不拦，攻击照常落地");
        std::cout << "  [T2] 613 非王系对照 通过\n";
    }

    //========== T3 1999：自身攻击对水系精灵无效 + 非 2 系对照 ==========
    {
        // 分支 A：对手水系 → 我的梦录攻击被无效（对手满血）
        {
            ScenarioBattle b(scenario::make_party(
                                 wuxin({kMengLu, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless,
                                                       kHarmless, kHarmless},
                                                      /*elem=*/{2, 0})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, 50000,
                     "T3a 核心：对手水系 → 1999 武装当回合我的攻击已被无效（0 伤）");
            std::cout << "  [T3a] 1999 对水系无效 通过\n";
        }
        // 分支 B：对手无系（0,0）→ 攻击照常
        {
            ScenarioBattle b(scenario::make_party(
                                 wuxin({kMengLu, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless,
                                                       kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp < 50000, "T3b 对照：非 2 系精灵 → 攻击正常落地");
            std::cout << "  [T3b] 1999 非水系对照 通过\n";
        }
    }

    //========== T4 1999 顺序敏感：对手换上水系精灵 → 当回合即时补票 ==========
    {
        // 对手队伍：0 号无系（在场）+ 1 号水系。第 1 回合验证攻击落地；
        // 第 2 回合对手主动切换水系上场 → 我后手的梦录攻击应被无效（EVENT_ENTER_STAGE 补票）。
        ElfPet normal_foe = foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless});
        ElfPet water_foe = foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                               /*elem=*/{2, 0});
        std::array<ElfPet, 6> party{normal_foe, water_foe, normal_foe,
                                    normal_foe, normal_foe, normal_foe};
        ScenarioBattle b(scenario::make_party(
                             wuxin({kMengLu, kMengLu, kHarmless, kHarmless, kHarmless},
                                   /*speed=*/50)),   // 我后手：换宠后攻击
                         party);
        b.start();
        BattleContext& ctx = b.ctx();
        // 第 1 回合：对手无系在场 → 我的攻击照常（打在无系宠身上）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp < 50000, "T4 前置：无系对手在场时攻击落地");

        // 第 2 回合：对手切水系 → 我后手用梦录 → 攻击应被无效
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.on_stage[1] == 1, "T4 前置：水系宠已登场");
        CHECK_EQ(ctx.getPet(1).hp, 50000,
                 "T4 核心：换上水系当回合我方攻击即被无效（watcher 即时补票）");
        std::cout << "  [T4] 1999 换宠即时重评 通过\n";
    }

    std::cout << "scenario_161 all passed" << std::endl;
    return 0;
}
