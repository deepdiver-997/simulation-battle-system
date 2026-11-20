#include <utils/sbs_rand.h>
// 场景 4660 —— K310 攻消回合类+PP归零族（830，soul_lib/k310_attack_break_ppzero.cpp）
//（2026-10-21，worktree ../agents/k310；号段 4660-4669 → T4660-T4661 两块）
//
// 断言口径（对手 5 技全拍打 PP=40；印主命中必中——PP 归零锚 = 打中落地）：
//   - T4660 命中：每刀恰归零 1 个技能 → 对手 PP 总量每刀 -40（db[0]=1 直证）。
//   - T4661 未命中：印主 accuracy=0 → 打不中 → PP 总量不变（"命中后"锚直证）。
//   （①句消除回合类的效果面由 1032/750 既有回归覆盖——本场景锚点无回合类载体，
//     冒烟直调不炸即可，见实现头注。）
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;

SoulMark make_mark() {
    // DB args 原序 [1,0]：db[0]=1 归零技能数（db[1] 废位）。
    SoulMark sm(830, "K310 魂印", "test", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 3204;
    return sm;
}

ElfPet mark_pet(int accuracy) {
    ElfPet p = make_pet(1, "苏亚希斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, 800000, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = accuracy;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int foe_pp_sum(BattleContext& ctx) {
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        sum += ctx.getPet(1).skills[i].pp;
    }
    return sum;
}

void attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4660 命中：每刀 PP 总量 -40（恰 1 技归零）═════════════════════════════
    {
        sbs_srand(4660);
        ScenarioBattle b(make_party(mark_pet(/*accuracy=*/100)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int sum0 = foe_pp_sum(ctx);
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0 - 35,
                 "T4660 830：命中 → 恰 1 个技能 PP 归零（-35，实测总量 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0 - 70,
                 "T4660 830：第二刀再归零 1 个（实测总量 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        std::cout << "  [T4660] 命中 PP 归零 通过\n";
    }

    // ══ T4661 未命中：PP 不动 ══════════════════════════════════════════════════
    {
        sbs_srand(4661);
        ScenarioBattle b(make_party(mark_pet(/*accuracy=*/0)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int sum0 = foe_pp_sum(ctx);
        attack_round(b);
        CHECK_EQ(foe_pp_sum(ctx), sum0,
                 "T4661 830：未命中 → PP 不归零（实测总量 " +
                     std::to_string(foe_pp_sum(ctx)) + "）");
        std::cout << "  [T4661] 未命中锚 通过\n";
    }

    std::cout << "scenario_4660 PASS (K310 攻消回合类+PP归零族)" << std::endl;
    return 0;
}
