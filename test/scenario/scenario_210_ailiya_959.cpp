#include <utils/sbs_rand.h>
// 场景 210：959 消除对手回合类效果，消除成功则自身全属性+{0}
// （2026-09-28，night-3 / D5 艾丽娅 3403；实现 resources/moves_lib/d5_D5.cpp）
//
// 载体：侍君天尽头 30931（真身技能，[959,582]/[1,2]；582 已有实现不在本测面）。
// T1 消除成功：给对手挂回合类效果（弹控票）→ 用 30931 → 断回合成功 → 自身 6 槽
//     能力等级全 +1，对手回合类效果被消除（消费）。
// T2 消除失败（对手无回合类效果）：能力等级纹丝不动（对照）。
// T3 重复成功可叠加：再挂回合类 → 再用 → 等级到 +2。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;      // 拍打（物攻 40）
constexpr int kShiJun = 30931;   // 侍君天尽头：[959,582] [1,2]

ElfPet ailiya(int skill) {
    ElfPet p = scenario::make_pet(3403, "艾丽娅", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < 6; ++i) {
        sum += ctx.ws.view_levels[side][i];
    }
    return sum;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 消除成功 → 自身全属性+1，对手回合类效果被消除 ==========
    {
        ScenarioBattle b(scenario::make_party(ailiya(kShiJun)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_reflect(1, /*source_slot=*/-1, /*source_effect_id=*/999,
                                       /*target=*/1, /*rounds=*/3);
        CHECK(ctx.rule_center_.has_round_type(1), "T1 前提：对手处于回合类效果");
        one_round(b);
        CHECK(!ctx.rule_center_.has_round_type(1), "T1 对手回合类效果被消除");
        CHECK_EQ(level_sum(ctx, 0), 6, "T1 消除成功：自身 6 槽能力等级全 +1");
        CHECK_EQ(level_sum(ctx, 1), 0, "T1 对手能力等级不动");
        std::cout << "  [T1] 消除成功 → 全属性+1 通过\n";
    }

    //========== T2 消除失败（对手无回合类效果）→ 不加 ==========
    {
        ScenarioBattle b(scenario::make_party(ailiya(kShiJun)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.rule_center_.has_round_type(1), "T2 前提：对手无回合类效果");
        one_round(b);
        CHECK_EQ(level_sum(ctx, 0), 0,
                 "T2 对手无回合类可消（NO_EFFECTS）→ 自身不加");
        std::cout << "  [T2] 消除失败不加 通过\n";
    }

    //========== T3 重复成功叠加：第二次成功 → +2 ==========
    {
        ScenarioBattle b(scenario::make_party(ailiya(kShiJun)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);
        one_round(b);
        CHECK_EQ(level_sum(ctx, 0), 6, "T3 第一次成功 +1");
        ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);
        one_round(b);
        CHECK(ctx.rule_center_.has_round_type(1) == false, "T3 第二次消除同样成功");
        CHECK_EQ(level_sum(ctx, 0), 12, "T3 两次成功叠加到 +2");
        std::cout << "  [T3] 重复成功叠加 通过\n";
    }

    std::cout << "scenario_210 all passed" << std::endl;
    return 0;
}
