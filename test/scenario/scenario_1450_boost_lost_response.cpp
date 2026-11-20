// 场景 1450 —— J59 被消响应族（2 宏，moves_lib/j_boost_lost_response.cpp）
//（2026-10-03，worktree ../agents/j59；号段 1450-1459 → T1450-T1451 两块）
//
// 断言口径：
//   - "被消/被吸"用原语直呼制造（clear_stat_boosts=kind 0 / transfer_stat_boosts=
//     kind 1）——⚠️ 直呼 emit 是**入队**，下一次 FSM run 才派发给 watcher（1109
//     同法），每次触发后必须 drive 一回合再断言；
//   - 804 后果 = 对手六槽精确 -1（kind 0，R2 派发）、再 -1（kind 1，R3 派发，槽 0
//     被转移提升污染、断槽 1-5）；过期（[3] 窗）R4 消除+派发不再响应；
//   - 998 后果 = R2 派发授予封属 2 回合（R2、R3）：R3 对手高速移动（20017=速度+2）
//     被封（速度 0）、R4 窗过落地（+2）；效果 4 若未实现对照会响亮失败，不会假绿。
//   - 载体全纯链（37052 [804]|[3,1] / 26797 [998]|[3,2]）——无兄弟链激活风险。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40
constexpr int kWuXi    = 37052;  // 无息破碎 [804]|[3,1]：物140（全属性降载体）
constexpr int kChaoNeng = 26797; // 超能法界 [998]|[3,2]：属（封属载体）
constexpr int kGaoSu   = 20017;  // 高速移动 [4]|[4,100,2]：属（速度+2，封属观察）
constexpr int kMaxHp   = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void drive_none_round(ScenarioBattle& b, int self_skill_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1450 804 自强被消/被吸 → 对手全属性-{1}（无息破碎 [3,1]）══
    // ⚠️ 原语直呼的 emit 是"入队"——下一次 FSM run 才派发给 watcher（1109 同法），
    // 每次触发后都要 drive 一回合再断言；过期分支同理必须真实派发过才算数。
    {
        ScenarioBattle b(
            make_party(mk_self({kWuXi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, 2);            // 自身预置提升（待被消）
        drive_none_round(b, 0);                       // R1 挂窗（[3] → R1..R3）
        // a) kind=0 消除 → 派发于 R2 → 对手六槽 -1
        (void)clear_stat_boosts(&ctx, 0);
        drive_none_round(b, 1);                       // R2（派发）
        for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
            CHECK_EQ(ctx.ability_levels[1][slot], -1,
                     "T1450.a 804：被消除 → 对手槽 " + std::to_string(slot) + " =-1（实测 " +
                         std::to_string(ctx.ability_levels[1][slot]) + "）");
        }
        // b) kind=1 吸取 → 派发于 R3 → 再 -1（槽 0 被转移提升污染，断槽 1-5）
        (void)stat_change(&ctx, 0, 0, 2);
        (void)transfer_stat_boosts(&ctx, 0, 1);
        drive_none_round(b, 1);                       // R3（派发，窗内最后一回合）
        for (int slot = 1; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
            CHECK_EQ(ctx.ability_levels[1][slot], -2,
                     "T1450.b 804：被吸取 → 对手槽 " + std::to_string(slot) + " =-2（实测 " +
                         std::to_string(ctx.ability_levels[1][slot]) + "）");
        }
        // c) 窗过期（[3] → R1..R3）：R4 消除 + R4 派发（已出窗）→ 不再响应
        (void)stat_change(&ctx, 0, 0, 2);
        (void)clear_stat_boosts(&ctx, 0);
        drive_none_round(b, 1);                       // R4（派发，窗已过期）
        bool changed = false;
        for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
            changed |= ctx.ability_levels[1][slot] < -2;
        }
        CHECK(!changed, "T1450.c 804：窗过期（R4 消除+派发）不再全属性降");
        std::cout << "  [T1450] 804 被消全属性降 通过\n";
    }

    // ══ T1451 998 自强被消 → 对手属性技命中失效（超能法界 [3,2]）══
    // 封属同样在"下一次 FSM run"才真正授予 → R1 末消除、R2 空转（授予落在 R2）、
    // R3 对手属性技被封、R4 窗过落地（R2 起算 2 回合=R2、R3）。
    {
        ScenarioBattle b(
            make_party(mk_self({kChaoNeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_none_round(b, 0);                       // R1 挂窗
        (void)stat_change(&ctx, 0, 0, 2);
        (void)clear_stat_boosts(&ctx, 0);             // 触发（入队）
        drive_none_round(b, 1);                       // R2 空转（派发，封属 R2、R3）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();                                    // R3：对手高速移动（被封）
        CHECK_EQ(ctx.ability_levels[1][4], 0,
                 "T1451.a 998：封属窗内对手属性技失效（速度实测 " +
                     std::to_string(ctx.ability_levels[1][4]) + "）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();                                    // R4：窗过，落地
        CHECK_EQ(ctx.ability_levels[1][4], 2,
                 "T1451.a 998：封属窗过 → 属性技落地（速度实测 " +
                     std::to_string(ctx.ability_levels[1][4]) + "）");
        std::cout << "  [T1451] 998 被消封属 通过\n";
    }

    std::cout << "scenario_1450 PASS (J59 被消响应族 804/998)" << std::endl;
    return 0;
}
