// 场景 6300 —— J297 未受击门封属性族（1031，moves_lib/j_untouched_seal_attr.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6300-6309）
//
// 断言口径：
//   - 1031（19925 幽茫千世决，纯链 [1031]|[1]）：门 = 本回合未受任意伤害；
//     门开 → 对手下 1 回合属性技能无效（seal_skill 属性回合型）。
//   - 观测面 = 自定义槽 1 的等级（对手瞪眼 20004 = effect 5 [1,100,-1]，实写引擎槽 1）：
//     被封 → 槽 1 仍 0；解封 → 槽 1 -1。**不用等级和**（坑集 §1）。
//   - 受击闭门用**第二回合**（对手先手拍打）验证，绕开"首施放回合计旗盲区"
//     （1708 记档）——R2 挨打后己方再施放，R2 末门闭。
//   - 审计槽断言门/旗：991031200+owner（门 1/0）、991031201+owner（旗 0/1）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1031 = 19925;   // 幽茫千世决 [1031]|[1] 物150（纯链）
constexpr int kPaiDa = 10004;         // 拍打：物 40
constexpr int kDengYan = 20004;       // 瞪眼：属性技，对手防御 -1

constexpr int kGateKey = 991031200;   // +owner
constexpr int kFlagKey = 991031201;   // +owner
// 瞪眼 effect 5 实写引擎槽 = args[0]（本载体 [1,100,-1] → 槽 1），
// 与官方码序无关；观测只看"该槽是否被改"。
constexpr int kObsSlot = 1;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int hp = 5000) {
    ElfPet p = make_pet(1, "己方", {kCarrier1031, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int speed = 100, int hp = 20000) {
    ElfPet p = make_pet(2, "对手", {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6300.a 门开：R1 未受击施放 → R2 对手属性技被封 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kFlagKey + 0), 0, "T6300.a R1 未受击（旗 0）");
        CHECK_EQ(stor(ctx, kGateKey + 0), 1, "T6300.a 门开（封属性 1 回合）");

        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);         // R2 对手属性技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kObsSlot], 0,
                 "T6300.a 瞪眼被封 → 己方槽1 仍 0（实测 " +
                     std::to_string(ctx.ability_levels[0][kObsSlot]) + "）");
    }

    // ══ T6300.b 窗界：R2 被封、R3 解封（duration={0}=1）══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);         // R2 瞪眼（封）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kObsSlot], 0, "T6300.b R2 仍在窗内 → 槽1 0");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);         // R3 瞪眼（解封）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kObsSlot], -1,
                 "T6300.b R3 窗外 → 槽1 -1（实测 " +
                     std::to_string(ctx.ability_levels[0][kObsSlot]) + "）");
    }

    // ══ T6300.c 受击闭门：R2 对手先手拍打 → 己方施放时旗=1 → R2 末不封 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*speed=*/400)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // R2：对手先手拍打（己方受击），随后己方再施放载体。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.drive();
        CHECK_EQ(stor(ctx, kFlagKey + 0), 1, "T6300.c R2 已受击（旗 1）");
        CHECK_EQ(stor(ctx, kGateKey + 0), 0, "T6300.c 门闭（不封属性）");
        // R3 对手瞪眼：R2 未发新封票、R1 封票已过期 → 允许。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kObsSlot], -1,
                 "T6300.c 门闭后属性技正常（槽1 -1，实测 " +
                     std::to_string(ctx.ability_levels[0][kObsSlot]) + "）");
    }

    // ══ T6300.d 封属性不封攻击：R2 对手拍打照常造成伤害 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);         // R2 对手攻击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.drive();
        const int loss = hp_before - ctx.getPet(0).hp;
        CHECK(loss > 0, "T6300.d 攻击技不受封属性影响（实测掉血 " +
                            std::to_string(loss) + "）");
    }

    std::cout << "scenario 6300 OK" << std::endl;
    return 0;
}
