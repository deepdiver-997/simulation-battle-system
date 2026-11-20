// 场景 580 —— E12 反伤族（462 / 580，moves_lib/e_reflect.cpp）
//（2026-09-29，worktree e12）
//
// 机制：EVENT_TAKE_DAMAGE watcher（TEAM scope）+ damage_kind==NORMAL 过滤
//（内核随本任务补 BattleEvent.damage_kind 字段——以前三档伤害共用同事件，
// "受攻击"没法从事件里判）。
//
// 断言组（反弹固伤无随机，可精确等式）：
//   T1 462（医疗之心 24606，args=[3,200]）：挂盾后挨打 → 对手恰掉 200；
//   T2 580（星海停驻 27796，args=[3,40]）：挂盾后挨打 d → 对手恰掉 d×40%；
//   T3 对照：不挂盾挨打 → 对手不掉血（回敬确由 watcher 引起）。
// 粉伤不反弹（damage_kind 过滤）：毒/烧 tick 路径为场景外内核 emit 点，
// 过滤逻辑与 T1 同一分支——记档不单测。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYiliao = 24606;     // 医疗之心（蒙琪）：属性技 [462,440] 462切片=[3,200]
constexpr int kXinghai = 27796;    // 星海停驻（时钟诛机）：属性技 [580] args=[3,40]
constexpr int kZhuangJi = 10001;   // 撞击
constexpr int kPassive = 10938;    // 本能一击：威力 0（挂盾方填充技——普攻会污染对手掉血断言）

ElfPet shielder(int skill, int speed = 300) {
    ElfPet p = make_pet(1, "挂盾方", {skill, kPassive, kPassive, kPassive, kPassive},
                        speed, /*atk=*/1, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet striker(int speed = 100) {
    ElfPet p = make_pet(2, "攻击方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, /*atk=*/100, /*hp=*/1000000);
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

    // ════════════════════════════════════════════════════════════════
    // T1 462：挂盾（3回合反弹200）→ R2 挨打 → 对手恰掉 200
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kYiliao)), make_party(striker()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 医疗之心
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 挂盾方不出手（零污染）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手攻击 → 触发反弹
        b.drive();
        const int foe_drop = foe_before - ctx.getPet(1).hp;
        CHECK_EQ(foe_drop, 200, "T1 核心：受攻击 → 反弹恰好 200 点固定伤害");
        std::cout << "  [T1] 462 固定反弹 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 580：挂盾（3回合反馈40%）→ R2 挨打 d → 对手恰掉 d×40%
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kXinghai)), make_party(striker()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 星海停驻
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int self_before = ctx.getPet(0).hp;
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 挂盾方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手攻击 → 触发反馈
        b.drive();
        const int taken = self_before - ctx.getPet(0).hp;
        const int foe_drop = foe_before - ctx.getPet(1).hp;
        CHECK(taken > 0, "T2 前置：确实挨到了红伤");
        CHECK_EQ(foe_drop, taken * 40 / 100,
                 "T2 核心：反馈量 = 受到的直接攻击伤害 × 40%");
        std::cout << "  [T2] 580 百分比反馈 通过 (taken=" << taken
                  << " foe_drop=" << foe_drop << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 对照：不挂盾挨打 → 对手不掉血
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(shielder(kZhuangJi)), make_party(striker()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(foe_before - ctx.getPet(1).hp, 0,
                 "T3 对照：无盾不反弹（回敬确由 watcher 引起）");
        std::cout << "  [T3] 无盾对照 通过\n";
    }

    std::cout << "scenario_580 PASS (E12 反伤族 462/580)" << std::endl;
    return 0;
}
