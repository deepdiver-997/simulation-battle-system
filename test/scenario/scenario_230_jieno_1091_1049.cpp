#include <utils/sbs_rand.h>
// 场景 230：1091 未击败对手则自身下{0}回合免疫并反弹异常 ／ 1049 击败对手则自身全属性+{0}
// （2026-09-28，night-3 / D7 杰诺 3408；实现 resources/moves_lib/d5_D7.cpp）
//
// 载体：击穿次元壁 30961（真身技能，[781,1091,1049]/[2,2,1]；781 已有实现不在本测面；
//       同技能 582 未击败必暴在 R1"未击败"路径会顺带武装，不影响本测断言面）。
// T1 未击败：R1 用 30961（对手存活）→ 当回合不授予（R1 无票）；R2 ROUND_START 授
//     双票 → R2/R3 免疫+反弹烧伤（apply_anomaly 原语直打：REFLECTED、对面烧伤、自己干净）；
//     R4 票过期 → 烧伤直接落上（SUCCESS）。
// T2 击败：对手 hp=1 → R1 击杀 → 1049 桥在 AFTER_ACTION 直发全属性+1（6 槽 sum=6），
//     且 1091 不武装（R2 无 reflect 票）。
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

constexpr int kHit = 10004;      // 拍打（物攻 40）
constexpr int kJiChuan = 30961;  // 击穿次元壁：[781,1091,1049] [2,2,1]
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

ElfPet jieno(int skill) {
    ElfPet p = scenario::make_pet(3408, "杰诺", {skill, kHit, kHit, kHit, kHit},
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

void one_round(ScenarioBattle& b, int my_skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_skill_index);
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

    //========== T1 未击败 → 下2回合免疫并反弹，窗口 [R2,R3]，R4 过期 ==========
    {
        ScenarioBattle b(scenario::make_party(jieno(kJiChuan)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0);   // R1：30961 命中，对手存活 → 1091 桥武装
        CHECK_EQ(level_sum(ctx, 0), 0, "T1 未击败：1049 不给");
        CHECK(ctx.rule_center_.has_round_type(0) == false,
              "T1 当回合不授予（窗口从下回合起）");
        one_round(b, 1);   // R2：ROUND_START 已授票（rounds=2 → 覆盖 R2/R3）
        CHECK(ctx.rule_center_.has_round_type(0), "T1 R2 弹控票已生效");
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1),
                 ApplyAnomalyResult::REFLECTED, "T1 R2 烧伤被反弹");
        CHECK(ctx.has_active_abnormal_status(0, kBurn) == false, "T1 R2 自身未被烧伤");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T1 R2 施加方被弹中烧伤");
        {   // R3：窗口第二回合，仍覆盖（回合中段查——票在回合末扣减到期）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.step_past(State::BATTLE_ROUND_START);
            ctx.debug_step_mode = false;
            CHECK(ctx.rule_center_.has_round_type(0), "T1 R3 仍在窗口内");
            CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1),
                     ApplyAnomalyResult::REFLECTED, "T1 R3 仍反弹");
            b.drive();
        }
        one_round(b, 1);   // R4：窗口已过（R3 末扣减到期）
        CHECK(ctx.rule_center_.has_round_type(0) == false, "T1 R4 票已过期");
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T1 R4 烧伤直接落上");
        std::cout << "  [T1] 未击败 → 下2回合免疫反弹（R2/R3 弹、R4 过期）通过\n";
    }

    //========== T2 击败 → 1049 全属性+1 立即；1091 不武装 ==========
    {
        ScenarioBattle b(scenario::make_party(jieno(kJiChuan)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;   // 下一击必杀
        one_round(b, 0);   // R1：击杀
        CHECK_EQ(level_sum(ctx, 0), 6, "T2 击败对手：1049 全属性+1（6 槽 sum=6）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 守方替补登场
        one_round(b, 1);   // R2：若 1091 误武装，此处应有票
        CHECK(ctx.rule_center_.has_round_type(0) == false,
              "T2 击败路径：1091 未武装（无弹控票）");
        std::cout << "  [T2] 击败 → 全属性+1、1091 不给 通过\n";
    }

    std::cout << "scenario_230 all passed" << std::endl;
    return 0;
}
