#include <utils/sbs_rand.h>
// 场景 145：阿尔忒弥斯 4745 技能组缺口 —— 效果 2009+2274（神驰天苍 29004）+
// 1293+1655（风萧影抖 29005）（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 2009："若对手处于异常状态则{0}回合内令对手使用的属性技能无效"（args=[2]）。
//   实现 = 条件在生效时刻求值（has_any_active_abnormal_status）+ seal_skill 回合窗口型。
// 2274："恢复自身全部PP值，每恢复1点吸取对手{0}点体力"（args=[30]）。
//   实现 = pp_restore 补满（缺口和=恢复点数）+ 固伤/按实际回（点数吸血 1613/2548 口径）。
// 1293："免疫下{0}次对手的攻击，免疫成功则自身全属性+{1}"（args=[1,1]）。
//   实现 = 2006 实测口径范式：SEAL 次数盔（count=1、可穿、被穿也消耗）+
//   EVENT_SKILL_ARMOR_RESOLVED 按 grant_id 匹配 → blocked → 全属性+1。
// 1655："{0}回合内每回合结束时降低对手所有技能{1}点PP值并恢复自身所有技能{2}点PP值"
//   （args=[4,0,1]——{1}=0 即不削对手，结构解码见 batch_a_skills.cpp 文件头★）。
//   实现 = BATTLE_ROUND_END 窗口 tick（1165 同款注册面）。
//
// 断言组：
//   T1 2009 主路径+对照：对手带异常 → 2009 封属票在（rule_center entries）→ 对手属性技
//      被拦（锋利自攻+1 打不出来）；对手无异常 → 无票、属性技照常。
//   T2 2274 主路径：5 槽各缺 2 PP（缺口 10）→ 全部回满；对手掉 10×30=300 固伤、
//      自身按实际回血。PP 扣减点在 AFTER_ACTION、2274 在 SKILL_EFFECT——出手槽
//      回满后再被扣 1（顺序敏感：slot0 == maxPP-1，其余槽 == maxPP）。
//   T3 1293+1655 三回合：R1 用风萧影抖（ armor 挂上、ROUND_END 全技能 +1 PP——
//      出手槽先扣后补 == maxPP，顺序敏感）；R2 对手攻击被次数盔拦下（不掉血）且
//      "免疫成功"→ 全属性 +1；R3 盔已消耗 → 伤害落地（count=1 语义）。
//
// 确定性：全技能强制必中、暴击 0；无概率掷点 → 零 rand 消费（伤害浮动除外，无比值断言）。
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

constexpr int kShenchi = 29004;   // 神驰天苍：属性、必中、[2276, 433, 2274, 2009, 481]
constexpr int kFengxiao = 29005;  // 风萧影抖：属性、必中、[191, 1293, 1647, 1655]
constexpr int kHit = 10004;       // 拍打：物攻 40（对手真实攻击）
constexpr int kBuff = 20265;      // 锋利：属性技（自身攻击+1、无伤害）
constexpr int kHarmless = 10938;  // 本能一击：威力 0、无 side_effect
constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);  // 16

ElfPet artemis(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "阿尔忒弥斯", skills, /*speed=*/200, /*atk=*/200,
                                  /*hp=*/4000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/200,
                                  /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

bool has_seal_entry(const BattleContext& ctx, int effect_id, int target) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.target == target && t.source_effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 2009：对手处于异常 → 封属性技能 2 回合；无异常对照 ==========
    {
        {
            ScenarioBattle b(scenario::make_party(artemis({kShenchi, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(apply_anomaly(&ctx, 1, kBleed, 3, 0) == ApplyAnomalyResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 神驰天苍
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手锋利
            b.drive();
            CHECK(has_seal_entry(ctx, 2009, 1), "T1 核心：2009 封属票在（对手带异常）");
            CHECK_EQ(ctx.ability_level(1, 0), 0,
                     "T1 对手属性技被封：锋利的攻击+1 没打出来");
        }
        {
            ScenarioBattle b(scenario::make_party(artemis({kShenchi, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!has_seal_entry(ctx, 2009, 1), "T1 对照：对手无异常 → 不封");
            CHECK_EQ(ctx.ability_level(1, 0), 1, "T1 对照：锋利照常生效（攻击+1）");
        }
        std::cout << "  [T1] 2009 条件封属+对照 通过\n";
    }

    //========== T2 2274：PP 全回满 + 每点吸取对手 30 点体力 ==========
    {
        ScenarioBattle b(scenario::make_party(artemis({kShenchi, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3500;   // 预留回血空间（max 4000）
        int deficit = 0;
        for (Skills& s : ctx.getPet(0).skills) {
            if (s.maxPP >= 2) {
                s.pp = s.maxPP - 2;
                deficit += 2;
            }
        }
        REQUIRE(deficit == 10);   // T2 前置：5 槽各缺 2 PP
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 神驰天苍
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[1].pp, ctx.getPet(0).skills[1].maxPP,
                 "T2 未出手槽回满（各槽 maxPP 不同，各自回满）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP - 1,
                 "T2 顺序敏感：2274 在 SKILL_EFFECT 回满、PP 扣减点在 AFTER_ACTION"
                 "（晚于回满）→ 出手槽 maxPP-1");
        CHECK_EQ(ctx.getPet(1).hp, 100000 - 300, "T2 核心：每恢复1点吸取对手30点（10×30）");
        CHECK(ctx.getPet(0).hp > 3500, "T2 吸取按实际回血（对手无盾 → 等量 300）");
        std::cout << "  [T2] 2274 回满+吸取 通过（对手 " << ctx.getPet(1).hp << "）\n";
    }

    //========== T3 1293+1655：次数盔（免疫成功→全属性+1）+ 回合结束 PP 恢复 ==========
    {
        sbs_srand(5);
        ScenarioBattle b(scenario::make_party(artemis({kFengxiao, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kHit, kHit, kHit, kHit, kHit})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int max_pp = ctx.getPet(0).skills[0].maxPP;
        // R1：风萧影抖（盔挂上）；对手攻击 → 次数盔拦下（免疫成功 → 全属性+1）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 4000, "T3 R1：盔拦下对手攻击（未掉血）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_level(0, i), 1,
                     "T3 核心：免疫成功 → 自身全属性+1（含命中共 6 槽）");
        }
        CHECK_EQ(ctx.getPet(0).skills[0].pp, max_pp,
                 "T3 R1 顺序敏感：PP 消耗在 AFTER_ACTION、1655 回复在 ROUND_END"
                 "（晚于消耗）→ 出手槽回到 maxPP");
        // R2：盔已消耗 → 对手攻击落地（count=1 语义）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < 4000, "T3 R2：次数盔已消耗 → 攻击照常落地");
        std::cout << "  [T3] 1293 次数盔+1655 PP 恢复 通过\n";
    }

    std::cout << "scenario_145 all passed" << std::endl;
    return 0;
}
