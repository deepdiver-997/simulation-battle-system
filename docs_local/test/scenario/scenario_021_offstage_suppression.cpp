// 场景 U：薇尔诗 2513 场下源抑制场域 + 瀚宇星皇 903 星皇之赐/之佑（B 主线）。
//
// 规格：docs/02-效果系统/魂印机制设计与精灵表现档案.md §4.0-4.4、
//       docs/06-交接/魂印与技能机制-交接.md §三。
//   2513："自身存活于出战阵容时，双方场下的精灵无法指定场上精灵为效果对象"
//   903 ："自身位于出战背包时，己方精灵拥有星皇之赐，己方首发精灵拥有星皇之佑"
//   判别轴：只挡"源是场下精灵"的效果；星皇自己首发（源场上）天然放行。
//
// 验证点：
//   U1 对照：无薇尔诗，星皇首发 → 每回合行动结束回 1/8 + 之佑免异常/免弱
//   U2 抑制：薇尔诗在场，星皇场下 → "印记还在但不回血"，之佑不授予
//   U3 放行：薇尔诗在场，星皇自己首发 → 回血与 之佑 天然放行
//   U4 死亡：星皇场下时薇尔诗被击杀 → 死亡监控清场域 → 场下星皇回血恢复
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动（威力 0，必中，属性技能——回血观察用）
constexpr int kSkillYanFan = 10037;       // 燕返（威力 60，必中——U4 击杀用）

constexpr int kMaxHp = 5000;
constexpr int kDamagedHp = 4000;  // 低于上限，让 1/8 回血（625）在数值上可见
constexpr int kExpectedAfterHeal = kDamagedHp + kMaxHp / 8;  // 4625

SoulMark make_star_mark() {
    // ⚠️ 必须走带 id 的构造函数：程序链路的 manager 查找（getSoulMarkProgram/hooks）
    // 只在构造函数里做；默认构造后手写字段不会接上插件注册的程序（has_program_ 恒 false）。
    SoulMark sm(903, "瀚宇星皇",
                "自身位于出战背包时，己方精灵拥有星皇之赐，首发精灵拥有星皇之佑",
                EffectArgs(std::vector<int>{50, 50, 8, 3, 1}));  // 对齐 effect_icon args
    return sm;
}

SoulMark make_veers_mark() {
    SoulMark sm(2513, "薇尔诗",
                "自身存活于出战阵容时，双方场下的精灵无法指定场上精灵为效果对象",
                EffectArgs(std::vector<int>{1, 0}));  // 对齐 effect_icon args
    return sm;
}

bool has_star_bless(scenario::ScenarioBattle& battle, int owner) {
    return battle.ctx().is_immune(owner, ImmunityType::ANOMALY, State::BATTLE_ROUND_START)
        && battle.ctx().is_immune(owner, ImmunityType::STAT_DROP, State::BATTLE_ROUND_START);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto make_star_pet = [](int speed) {
        return scenario::make_pet(903903, "瀚宇星皇",
                                  {kSkillGaoSuYiDong, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                   kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                  speed, /*atk=*/100, kMaxHp, make_star_mark());
    };
    auto make_plain_pet = [](int speed, int atk) {
        return scenario::make_pet(100100, "普通精灵",
                                  {kSkillGaoSuYiDong, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                   kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                  speed, atk, kMaxHp);
    };
    auto make_veers_pet = [](int speed) {
        return scenario::make_pet(251251, "薇尔诗",
                                  {kSkillGaoSuYiDong, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                   kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                  speed, /*atk=*/100, kMaxHp, make_veers_mark());
    };

    // ---- U1：对照（无薇尔诗，星皇首发）——回血 + 之佑都在 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(make_star_pet(300)),
                                        scenario::make_party(make_plain_pet(100, 100)));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.getPet(0).hp = kDamagedHp;  // 构造后经 ctx 改（ElfPet 拷贝构造会从数值重推 hp）

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        assert(ctx.getPet(0).hp == kExpectedAfterHeal);
        assert(has_star_bless(battle, 0));
        assert(!ctx.block_offstage_to_onstage);
        std::cout << "  [U1] 对照无薇尔诗: hp " << kDamagedHp << "->" << ctx.getPet(0).hp
                  << "（之赐回 1/8），之佑免异常/免弱已授予，场域关" << std::endl;
    }

    // ---- U2：薇尔诗在场 + 星皇场下 —— 抑制（"印记还在，但不回血"） ----
    {
        auto normal = make_plain_pet(300, 100);
        auto star = make_star_pet(300);
        std::array<ElfPet, 6> party0{normal, star, normal, normal, normal, normal};
        scenario::ScenarioBattle battle(party0,
                                        scenario::make_party(make_veers_pet(100)));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.getPet(0).hp = kDamagedHp;
        assert(ctx.find_pet_with_soulmark(0, 903) == 1);   // 星皇在 1 号槽（场下）
        assert(ctx.block_offstage_to_onstage);             // 2513 场域开（ROSTER：她首发即开）

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        assert(ctx.getPet(0).hp == kDamagedHp);  // 场下星皇 → 之赐被抑制
        assert(!has_star_bless(battle, 0));      // 场下星皇 → 之佑不授予
        std::cout << "  [U2] 薇尔诗在场+星皇场下: hp 保持 " << ctx.getPet(0).hp
                  << "（之赐抑制），之佑未授予，场域开" << std::endl;
    }

    // ---- U3：薇尔诗在场 + 星皇自己首发 —— 源场上，天然放行 ----
    {
        scenario::ScenarioBattle battle(scenario::make_party(make_star_pet(300)),
                                        scenario::make_party(make_veers_pet(100)));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.getPet(0).hp = kDamagedHp;

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        assert(ctx.block_offstage_to_onstage);           // 场域开着（薇尔诗在场）
        assert(ctx.getPet(0).hp == kExpectedAfterHeal);  // 但源（星皇）在场上 → 放行
        assert(has_star_bless(battle, 0));               // 之佑同样放行
        std::cout << "  [U3] 薇尔诗在场+星皇首发: hp " << kDamagedHp << "->" << ctx.getPet(0).hp
                  << "（源场上放行），之佑已授予" << std::endl;
    }

    // ---- U4：星皇场下 + 薇尔诗被击杀 → 死亡监控清场域 → 回血恢复 ----
    {
        auto attacker = scenario::make_pet(100101, "击杀手",
                                           {kSkillYanFan, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                            kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                           /*speed=*/300, /*atk=*/500, kMaxHp);
        auto star = make_star_pet(300);
        std::array<ElfPet, 6> party0{attacker, star, attacker, attacker, attacker, attacker};
        // 替补槽不带 2513（否则击杀首发后场域仍开着，U4 断言无意义）
        std::array<ElfPet, 6> party1{make_veers_pet(100), make_plain_pet(100, 100),
                                     make_plain_pet(100, 100), make_plain_pet(100, 100),
                                     make_plain_pet(100, 100), make_plain_pet(100, 100)};
        scenario::ScenarioBattle battle(party0, party1);
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.getPet(0).hp = kDamagedHp;
        ctx.getPet(1).hp = 1;  // 薇尔诗残血，燕返必中一刀带走

        // R1：击杀薇尔诗（先手），星皇之赐因"场域开 + 星皇场下"被抑制
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        battle.drive();  // 跑到 CHOOSE_AFTER_DEATH（等 1 号方选替补）

        assert(ctx.getPet(1).hp <= 0);                 // 薇尔诗被击杀
        assert(!ctx.block_offstage_to_onstage);        // 死亡监控清场域（独立于魂印开闭路径）
        assert(ctx.getPet(0).hp == kDamagedHp);        // R1 之赐仍被抑制（当时场域还开着）
        battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);

        // R2：场域已关 → 场下星皇的之赐恢复
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        battle.drive();

        assert(ctx.getPet(0).hp == kExpectedAfterHeal);
        std::cout << "  [U4] 击杀薇尔诗后: 场域清除，R2 回血恢复 " << kDamagedHp << "->"
                  << ctx.getPet(0).hp << std::endl;
    }

    std::cout << "scenario_u PASS (2513 场下源抑制场域 + 903 星皇之赐/之佑：对照/抑制/放行/死亡恢复)"
              << std::endl;
    return 0;
}
