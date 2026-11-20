// 场景 059：通用特性·被动毒（带电/高热/冰冷/阴森，Eid 6 家族）。
//
// 口径来源：new_se stat=1（Eid 6：带电0/高热2/冰冷5/阴森6）+ 用户 2026-09-16 拍板：
//   - 描述"受到**普通攻击**时有 n% 使对方XX"；**普通攻击 = 物攻**（用户确认）。
//   - 施加对象 = **攻方**（谁打的就弹给谁）；概率 args[0] = 百分点（3~8）、
//     args[1] = 异常码（battle_effects 命名空间）；赋予回合随机 2~3。
//   - **遗留裸施加**（用户拍板）：走 apply_anomaly_raw —— **什么都不检测**，
//     免疫（含 Ancient 层 / Mark 0 老魂免）/弹控/异常抗性/转化 全部穿透。
//     这是比主动毒更早的遗留机制（被动毒在语料里与主动毒并列，且不受
//     "不高于50%异常附加效果失效"影响——说明它不是普通效果路径）。
//   - 时点：攻方物理攻击**命中**后、技能效果结算之前（BATTLE_*_ON_SKILL_HIT，
//     与主动毒同钩位；"受到普通攻击时"需要这次攻击命中）。
//
// 验证点：
//   Q1 带电·物攻命中 → 攻方麻痹（弹给打人的人，不是守方自己）
//   Q2 裸施加穿透 Ancient 层免疫票（48 式）——"什么都不检测"
//   Q3 裸施加穿透 Mark 0 老魂免兜底
//   Q4 裸施加穿透弹控（191 式 REFLECT：不反弹、守方也不中招、攻方照中）
//   Q5 裸施加穿透异常抗性（100% 麻痹抗性）
//   Q6 裸施加穿透转化（麻痹→烧伤：攻方中的是麻痹不是烧伤）
//   Q7 特攻不触发（"受到普通攻击"=物攻）；属性技能不触发
//   Q8 阴森（6=害怕）走同一钩子：物攻命中 → 攻方害怕
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <effects/trait_state.h>
#include <entities/mark.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40 必中无效果
constexpr int kShuiQiang = 10013;  // 水枪：特攻 40 必中无效果
constexpr int kGaoSu = 20017;      // 高速移动：属性技能

using scenario::ScenarioBattle;

// 攻方：无特性（被动毒在守方身上）
ElfPet plain_attacker(const std::vector<int>& skills = {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                      int speed = 200) {
    return scenario::make_pet(0, "攻方",
                              {skills[0], skills[1], skills[2], skills[3], skills[4]},
                              speed, 100, 400);
}
// 守方：带被动毒（args[0]=100 → 100% 触发保确定性；args[1]=异常码）
ElfPet passive_defender(const char* trait_name, int status_code) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, 400,
                              {}, scenario::make_trait(trait_name, 5, {100, status_code}));
}

void strike(ScenarioBattle& b, int p0_skill = 0, int p1_skill = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, p0_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_skill);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
    const int burn = abnormal_status_id(AbnormalStatusId::Burn);
    const int fear = abnormal_status_id(AbnormalStatusId::Fear);

    //========== Q1 带电·物攻命中 → 攻方麻痹 ==========
    {
        ScenarioBattle b({scenario::make_party(plain_attacker())},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        strike(b);
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "Q1 攻方应被弹上麻痹");
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "Q1 守方自己不应中招");
        const int end = ctx.get_abnormal_status_end_round(0, paralysis);
        CHECK(end - 1 == 2 || end - 1 == 3, "Q1 赋予回合应为 2~3");
        std::cout << "Q1 PASS 带电弹给攻方（2~3 回合）\n";
    }

    //========== Q2 裸施加穿透 Ancient 层免疫票 ==========
    {
        ScenarioBattle b({scenario::make_party(plain_attacker())},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        BattleContext& ctx = b.ctx();
        // 攻方挂 Ancient 层全异常免疫票（48 式）→ 裸施加应无视
        ctx.grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0, 3, /*source_id=*/995,
                           false, EffectScope::ON_STAGE, 0, /*source_effect_id=*/995,
                           ImmunityTier::Ancient);
        strike(b);
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "Q2 裸施加应穿透 Ancient 层免疫");
        std::cout << "Q2 PASS 裸施加穿透 Ancient 层免疫\n";
    }

    //========== Q3 裸施加穿透 Mark 0 老魂免兜底 ==========
    {
        ElfPet atk = plain_attacker();
        atk.marks.push_back(Mark(0, "老魂免兜底"));
        ScenarioBattle b({scenario::make_party(atk)},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        strike(b);
        CHECK(b.ctx().has_active_abnormal_status(0, paralysis), "Q3 裸施加应穿透 Mark 0");
        std::cout << "Q3 PASS 裸施加穿透 Mark 0 老魂免\n";
    }

    //========== Q4 裸施加穿透弹控（不反弹、攻方照中）==========
    {
        ScenarioBattle b({scenario::make_party(plain_attacker())},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        BattleContext& ctx = b.ctx();
        // 攻方挂弹控（191 式：Modern 票 + REFLECT）→ 裸施加既不免疫也不反弹
        ctx.grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0, 3, /*source_id=*/191,
                           false, EffectScope::ON_STAGE, 0, /*source_effect_id=*/191,
                           ImmunityTier::Modern);
        ctx.rule_center_.grant_reflect(0, -1, 191, 0, 3);
        strike(b);
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "Q4 裸施加应穿透弹控直接落地");
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "Q4 不应把异常反弹回守方");
        std::cout << "Q4 PASS 裸施加穿透弹控（不反弹）\n";
    }

    //========== Q5 裸施加穿透异常抗性 ==========
    {
        ElfPet atk = plain_attacker();
        std::pair<int, int> slots[kResistanceSlotCount] = {{paralysis, 100}, {0, 0}, {0, 0}};
        CHECK(atk.resistance.setControlResistance(slots), "Q5 抗性槽应设置成功");
        ScenarioBattle b({scenario::make_party(atk)},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        strike(b);
        CHECK(b.ctx().has_active_abnormal_status(0, paralysis), "Q5 裸施加应穿透 100% 抗性");
        std::cout << "Q5 PASS 裸施加穿透异常抗性\n";
    }

    //========== Q6 裸施加穿透转化 ==========
    {
        ScenarioBattle b({scenario::make_party(plain_attacker())},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.anomaly_conversion[0][paralysis] = burn;  // 攻方预置 麻痹→烧伤
        strike(b);
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "Q6 裸施加应无视转化落麻痹");
        CHECK(!ctx.has_active_abnormal_status(0, burn), "Q6 不应被转成烧伤");
        std::cout << "Q6 PASS 裸施加穿透转化\n";
    }

    //========== Q7 特攻/属性技能不触发 ==========
    {
        ElfPet spec_atk = plain_attacker({kShuiQiang, kShuiQiang, kShuiQiang,
                                          kShuiQiang, kShuiQiang});
        ScenarioBattle b({scenario::make_party(spec_atk)},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        strike(b);
        CHECK(!b.ctx().has_active_abnormal_status(0, paralysis), "Q7 特攻不应触发被动毒");
        std::cout << "Q7a PASS 特攻不触发被动毒\n";
    }
    {
        ElfPet attr_atk = plain_attacker({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu});
        ScenarioBattle b({scenario::make_party(attr_atk)},
                         {scenario::make_party(passive_defender("带电", 0))});
        b.start();
        strike(b);
        CHECK(!b.ctx().has_active_abnormal_status(0, paralysis), "Q7 属性技能不应触发被动毒");
        std::cout << "Q7b PASS 属性技能不触发被动毒\n";
    }

    //========== Q8 阴森（6=害怕）同钩子 ==========
    {
        ScenarioBattle b({scenario::make_party(plain_attacker())},
                         {scenario::make_party(passive_defender("阴森", 6))});
        b.start();
        strike(b);
        CHECK(b.ctx().has_active_abnormal_status(0, fear), "Q8 阴森应令攻方害怕");
        std::cout << "Q8 PASS 阴森（害怕）同钩子\n";
    }

    std::cout << "\n场景 059 全部通过：通用特性·被动毒（带电/高热/冰冷/阴森）✓" << std::endl;
    return 0;
}
