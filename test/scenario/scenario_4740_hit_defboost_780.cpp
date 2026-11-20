// 场景 4740 —— K318 受击双防增益族（780，soul_lib/k318_hit_def_boost.cpp）
//（2026-10-21，worktree ../agents/k318；号段 4740-4749 → T4740-T4742 三块）
//
// 断言口径（780「受到攻击时自身防御+2、特防+2（boss无效）」）：
//   - T4740 **受击生效**：对手打中印主 → 印主防御槽 +2、特防槽 +2（精确）。
//   - T4741 **门闭对照**：对手**没打中**印主（对手 NONE）→ 印主双防不变。
//   - T4742 **无魂印对照**：同样被打中 → 印主双防不变（增益确来自本魂印）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ 断言读 **ctx.ability_levels[0][槽]**：stat_change 写本体
//   （battle_primitives.cpp:1402）+ 视图同步，两者同源。槽序（速查 §三）：
//   **2防 3特防**。
// ⚠️ **增益是持久的**（写 pet.levels/ability_levels，不在 ws 里）→ 跨回合累积，
//   故断言"恰好 +2"（首回合无其他增益源）。
// ⚠️ 事件面读数：EVENT_TAKE_DAMAGE 的 ev.amount = **实际扣血**——必须真的打中
//   （damage>0）才触发。
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

constexpr int kJi = 10001;    // 撞击：攻击载体
constexpr int kMaxHp = 99999;
constexpr int kDefSlot = 2;   // 槽 2 = 防御
constexpr int kSpDefSlot = 3; // 槽 3 = 特防

SoulMark make_mark() {
    // DB args 原序 [2,2,1]：db[0]=2（防御+2）、db[1]=2（特防+2）、db[2]=1 记档。
    SoulMark sm(780, "K318 魂印", "test", EffectArgs(std::vector<int>{2, 2, 1}));
    sm.monster_id = 3126;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "魔龙·加洛斯格", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/1, kMaxHp, make_mark())
        : make_pet(1, "裸魔龙", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手：快（先手）+ 会打。
ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/100, kMaxHp);
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

    // ══ T4740 受击生效：对手打中印主 → 防御+2 特防+2 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        REQUIRE(ctx.getPet(0).hp < kMaxHp);   // 前置：印主确实被打掉了血
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], 2,
                 "T4740 780：受到攻击 → 自身防御 +2（实测 "
                     + std::to_string(ctx.ability_levels[0][kDefSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kSpDefSlot], 2,
                 "T4740 780：受到攻击 → 自身特防 +2（实测 "
                     + std::to_string(ctx.ability_levels[0][kSpDefSlot]) + "）");
        std::cout << "  [T4740] 受击双防增益 通过\n";
    }

    // ══ T4741 门闭对照：对手没出手 → 双防不变 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], 0,
                 "T4741 780：未受到攻击 → 防御不变（实测 "
                     + std::to_string(ctx.ability_levels[0][kDefSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kSpDefSlot], 0,
                 "T4741 780：未受到攻击 → 特防不变（实测 "
                     + std::to_string(ctx.ability_levels[0][kSpDefSlot]) + "）");
        std::cout << "  [T4741] 未受击门闭对照 通过\n";
    }

    // ══ T4742 无魂印对照：被打中也不加双防 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        REQUIRE(ctx.getPet(0).hp < kMaxHp);   // 前置：确实被打中
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], 0,
                 "T4742 780：对照——不带魂印时被打中也不加防御（增益确来自本魂印）");
        CHECK_EQ(ctx.ability_levels[0][kSpDefSlot], 0,
                 "T4742 780：对照——不带魂印时被打中也不加特防");
        std::cout << "  [T4742] 无魂印对照 通过\n";
    }

    // ══ T4743 **对手受击不触发**（"自己受击"门的判别力）══
    // ⚠️ 变异测试 MUT1（去掉 ev.target != owner 门）在本场景里**测不出来**：
    //   印主从不出手 → ev.actor 永远不是 owner → 另一条 `ev.actor == owner` 守卫
    //   仍拦住了。真正的漏洞是"**对手**受击时印主也会加双防"——本块专测它：
    //   印主打对手 → 对手掉血（事件触发）→ 印主的双防**不应**变化。
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // 印主打对手
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        REQUIRE(ctx.getPet(1).hp < kMaxHp);   // 前置：对手确实掉了血（事件触发了）
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], 0,
                 "T4743 780：**对手**受击 → 印主不获得双防增益（'自己受击'门），实测防御 "
                     + std::to_string(ctx.ability_levels[0][kDefSlot]));
        CHECK_EQ(ctx.ability_levels[0][kSpDefSlot], 0,
                 "T4743 780：**对手**受击 → 印主特防也不加，实测 "
                     + std::to_string(ctx.ability_levels[0][kSpDefSlot]));
        std::cout << "  [T4743] 对手受击不触发 通过\n";
    }

    std::cout << "scenario_4740 PASS (K318 受击双防增益族)" << std::endl;
    return 0;
}
