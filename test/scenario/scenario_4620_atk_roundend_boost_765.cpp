// 场景 4620 —— K306 攻技回合末自增族（765，soul_lib/k306_atk_roundend_boost.cpp）
//（2026-10-21，worktree ../agents/k306；号段 4620-4629 → T4620-T4622 三块）
//
// 断言口径（765「所有攻击技能使自身在回合结束后攻击+1，速度+1」）：
//   - T4620 **门开**：印主用攻击技出手 → 回合结束后自身攻击槽 +1、速度槽 +1。
//   - T4621 **属性技门闭**：印主用**属性技**出手 → 回合结束**不**加（tips"所有攻击
//     技能"）。
//   - T4622 **不出手门闭**：印主整回合 NONE → 回合结束不加（"用技"是前提）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ 断言读 **ctx.ability_levels[0][槽]**：stat_change 写本体 `ability_levels`
//   （battle_primitives.cpp:1402）+ 视图同步（write_ability_level），两者同源。
//   槽序（速查 §三）：**0攻 4速**。
// ⚠️ 增益是**持久**的（写 pet.levels/ability_levels，不在 ws 里）→ 跨回合累积，
//   故 T4620 断言"恰好 +1"（首回合无其他增益源）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物理攻击技
constexpr int kSuoTou = 20004;  // 瞪眼：属性技
constexpr int kMaxHp = 99999;
constexpr int kAtkSlot = 0;    // 槽 0 = 攻击
constexpr int kSpeedSlot = 4;  // 槽 4 = 速度

SoulMark make_mark(int atk_delta = 1, int speed_delta = 1) {
    // DB args 原序 [1,1]：db[0]=1（攻击+1）、db[1]=1（速度+1）——双参自证。
    // ⚠️ 两下标**同值 1** → "读错下标"在这行数据上不可观测（数据巧合，非断言缺口）。
    //   故本文件带一个**参数化**的 mark_pet(2,3) 变体（见 T4623），把"读 db[0] 当
    //   攻击增量 / 读 db[1] 当速度增量"变成可观测行为。
    SoulMark sm(765, "K306 魂印", "test",
                EffectArgs(std::vector<int>{atk_delta, speed_delta}));
    sm.monster_id = 3104;
    return sm;
}

ElfPet mark_pet(int skill0 = kPaDa, int atk_delta = 1, int speed_delta = 1) {
    ElfPet p = make_pet(1, "龙翼战神", {skill0, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/100, kMaxHp,
                        make_mark(atk_delta, speed_delta));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
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

    // ══ T4620 门开：攻击技出手 → 回合末 攻+1 速+1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 1,
                 "T4620 765：用攻击技能 → 回合结束后攻击 +1（实测 "
                     + std::to_string(ctx.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1,
                 "T4620 765：用攻击技能 → 回合结束后速度 +1（实测 "
                     + std::to_string(ctx.ability_levels[0][kSpeedSlot]) + "）");
        std::cout << "  [T4620] 攻技回合末自增 通过\n";
    }

    // ══ T4621 属性技门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*skill0=*/kSuoTou)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 0,
                 "T4621 765：用属性技 → 回合结束不加（tips'所有攻击技能'），实测攻击 "
                     + std::to_string(ctx.ability_levels[0][kAtkSlot]) + " 速度 "
                     + std::to_string(ctx.ability_levels[0][kSpeedSlot]));
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4621 765：用属性技 → 速度槽也不加");
        std::cout << "  [T4621] 属性技门闭对照 通过\n";
    }

    // ══ T4622 不出手门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 0,
                 "T4622 765：整回合不出手 → 回合结束不加（'用攻击技能'是前提），实测攻击 "
                     + std::to_string(ctx.ability_levels[0][kAtkSlot]));
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4622 765：整回合不出手 → 速度槽也不加");
        std::cout << "  [T4622] 不出手门闭对照 通过\n";
    }

    // ══ T4623 增量由行参驱动（参数化）：db[0]=2 攻 / db[1]=3 速 ══
    // ⚠️ 真实行 db[0]=db[1]=1（**同值**）→ 读错下标不可观测。故用参数化变体
    //   （db[0]=2 / db[1]=3）把"各读各的下标"变成可观测行为——变异测试 MUT2
    //   （速度增量误读 db[0]）在此转红（实得 攻+2/速+2，期望 攻+2/速+3）。
    {
        ScenarioBattle b(make_party(mark_pet(/*skill0=*/kPaDa, /*atk_delta=*/2,
                                              /*speed_delta=*/3)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2,
                 "T4623 765：攻击增量 = db[0] = 2（实测 "
                     + std::to_string(ctx.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 3,
                 "T4623 765：速度增量 = db[1] = 3（实测 "
                     + std::to_string(ctx.ability_levels[0][kSpeedSlot]) + "）");
        std::cout << "  [T4623] 增量由行参驱动 通过\n";
    }

    std::cout << "scenario_4620 PASS (K306 攻技回合末自增族)" << std::endl;
    return 0;
}
