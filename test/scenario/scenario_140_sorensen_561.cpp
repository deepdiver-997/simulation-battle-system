// 场景 140：混沌魔君索伦森 3414 技能组缺口 —— 效果 561（堕世 22745）
//（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 561 官方模板（effect_info，args_num=0）："先出手时当回合对手使用技能后若自身体力为0，
// 则令自身体力等于最大体力"；4399 图鉴："若本回合受到致命伤害则强制存活并恢复所有体力"。
// 实现 = 死亡拦截器（复活档 sees_hp_consume=true），SKILL_EFFECT 时判"先出手"武装，
// 拦截时点在死亡漏斗内（先手方被后手打死 → SECOND_MOVER_DEATH 漏斗询问拦截器）——
// 宠物**留在场上**、体力=最大（比"回合末补血"强：不会先被死亡补位换下场）。
//
// 断言组：
//   T1 主路径：先手用堕世 → 被后手致死打击打穿 → 拦截复活、体力=最大、仍在场。
//   T2 对照：后手用堕世（不满足"先出手"）→ 正常死亡。
//   T3 对照：先手但没用堕世 → 正常死亡。
//   T4 窗口：只在施放当回合武装——第 2 回合再死不再复活（直调 defeat_pet 验证拦截器哑火）。
//
// 确定性：堕世 must_hit（DB）；对手拍打强制必中；暴击率 0；无概率掷点 → 零 rand 消费。
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

constexpr int kDuoshi = 22745;    // 堕世：属性、必中、effect [561]
constexpr int kHit = 10004;       // 拍打：物攻 40
constexpr int kHarmless = 10938;  // 本能一击：威力 0、无 side_effect（热身技）

ElfPet sorensen(int speed, int hp, std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "混沌魔君索伦森", skills, speed, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int atk, int hp) {
    ElfPet p = scenario::make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit}, /*speed=*/100,
                                  atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;   // 对手必中：去命中掷点（死亡时点确定）
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

    //========== T1 主路径：先手堕世 → 致死打击 → 复活满体力、留在场上 ==========
    {
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200, /*hp=*/500,
                                                       {kDuoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(foe(/*atk=*/3000, /*hp=*/4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 堕世
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 对手拍打
        b.drive();
        const int dealt = ctx.ws.raw_attack_damage[1];
        CHECK(dealt > 500, "T1 前提：对手这一击足以打死 500 体力的索伦森");
        CHECK_EQ(ctx.getPet(0).hp, 500,
                 "T1 核心：强制存活并恢复所有体力（体力=最大）");
        CHECK(ctx.on_stage[0] == 0, "T1 仍在场上（没有被死亡补位换下）");
        std::cout << "  [T1] 先手堕世复活 通过（致死伤 " << dealt << " → 满血 500）\n";
    }

    //========== T2 对照：后手用堕世 → 不满足"先出手"，拦截器不武装 ==========
    {
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/50, /*hp=*/500,
                                                       {kDuoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(foe(/*atk=*/10, /*hp=*/4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 第 1 回合：对手先手弱攻（打不死），索伦森后手用堕世——"先出手"前提不成立。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp > 0, "T2 前置：弱攻下存活");
        // 直调死亡漏斗（actor=对手、DAMAGE 因果）：拦截器未武装 → 死亡成立。
        ctx.getPet(0).hp = 0;
        const DefeatResult r = defeat_pet(&ctx, 0, ctx.on_stage[0], /*actor=*/1,
                                          DefeatCause::DAMAGE);
        CHECK_EQ(r, DefeatResult::DEFEATED,
                 "T2 核心：后手用堕世不武装（DEFEATED 而非 INTERCEPTED）");
        std::cout << "  [T2] 后手不武装对照 通过\n";
    }

    //========== T3 对照：先手但没用堕世 → 正常死亡 ==========
    {
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200, /*hp=*/500,
                                                       {kHit, kDuoshi, kHit, kHit, kHit})),
                         scenario::make_party(foe(/*atk=*/3000, /*hp=*/4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打（非堕世）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 对手拍打
        b.drive();
        CHECK(ctx.getPet(0).hp <= 0, "T3 没用堕世：无武装 → 正常死亡");
        std::cout << "  [T3] 未用堕世对照 通过\n";
    }

    //========== T4 窗口：只在施放当回合武装；第 2 回合拦截器哑火 ==========
    {
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200, /*hp=*/500,
                                                       {kDuoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(foe(/*atk=*/10, /*hp=*/4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 第 1 回合：堕世武装（对手威力 0 的热身不会致死）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 对手用拍打（atk=10 打不死）
        b.drive();
        CHECK(ctx.getPet(0).hp > 0, "T4 前置：第 1 回合存活（atk=10 打不死 500 血）");
        // 第 2 回合：没用堕世 → 拦截器应已过期。直调死亡漏斗原语验证（actor=对手、
        // 同为 DAMAGE 因果；若拦截器仍活着会返回 INTERCEPTED）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        ctx.getPet(0).hp = 0;
        const DefeatResult r = defeat_pet(&ctx, 0, ctx.on_stage[0], /*actor=*/1,
                                          DefeatCause::DAMAGE);
        CHECK_EQ(r, DefeatResult::DEFEATED,
                 "T4 核心：第 2 回合拦截器已哑火（死亡成立，DEFEATED 而非 INTERCEPTED）");
        std::cout << "  [T4] 当回合窗口过期 通过\n";
    }

    std::cout << "scenario_140 all passed" << std::endl;
    return 0;
}
