#include <utils/sbs_rand.h>
// 场景 146：混沌魔尊 3683 技能组缺口 —— 效果 1123（冥刃断灵 30868）+ 982（邪心黯天决
// 30872）+ 1367（混沌吞噬 26302）+ 938+731（无影禁锢 32395）
//（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 1123："吸取对手能力提升状态，吸取成功则下{0}回合自身所有技能先制+{1}"（args=[1,2]）。
// 982："{0}%使对手{1}，自身每处于一种能力提升状态则概率提高{2}%"
//   （args=[40,7,10]：基础 40% 疲惫、每种提升 +10%）。
// 1367："随机附加{0}种异常状态，未触发则{1}回合内令对手使用的属性技能无效"
//   （args=[2,2]；"未触发"取 landed==0 保守档）。
// 938："使对手全属性-{0}，先出手时效果翻倍"（args=[1]）。
// 731："本回合打出致命一击则下{0}回合其它攻击技能必定致命一击"（args=[1]）。
//
// 断言组：
//   T1 1123 主路径+双窗口顺序：吸取（我方+1/对手清零）→ R2 MOVE_RIGHT 自身先制+2，
//      同刻 867（已有）"消回合成功→对手先制-2"的窗口也在——同桶同刻各自落账、互不覆盖。
//   T2 982 概率标尺：6 种提升 → 40+60=100%（roll_percent 短路、确定性落地）；
//      无提升 → 基础 40%（种子分支）。
//   T3 1367 两分支：正常 → 随机 2 种异常落地（恰 2 条活跃）；对手挂异常免疫 →
//      landed==0 → "未触发"补偿：2 回合封属票在。
//   T4 938+731：必暴（rate=100 不掷点）→ 对手 6 槽全 -2（先手翻倍）+ R2 must_crit
//      置位且攻击必暴；对照：非必暴 → 无 must_crit。
//
// 确定性：全技能强制必中、暴击率 0（T4 显式开满）。
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

constexpr int kMingren = 30868;   // 冥刃断灵：物攻 85、先 3、[1123, 867]
constexpr int kXiexin = 30872;    // 邪心黯天决：物攻 150、[48, 982]
constexpr int kTunshi = 26302;    // 混沌吞噬：属性、必中、[585, 433, 971, 1367]
constexpr int kWuying = 32395;    // 无影禁锢：物攻 150、[938, 731]
constexpr int kHarmless = 10938;  // 本能一击：威力 0
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 7 疲惫

ElfPet mozun(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "混沌魔尊", skills, /*speed=*/200, /*atk=*/300,
                                  /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/50,
                                  /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int count_active_anomalies(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
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

    //========== T1 1123：吸取成功 → 下 1 回合自身先制+2（与 867 同刻并存） ==========
    {
        ScenarioBattle b(scenario::make_party(mozun({kMingren, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*特攻=*/1, 1) == StatChangeResult::SUCCESS);
        // 对手挂回合类效果（弹控票）→ 后继 867（已有实现）"消除成功"分支也走通
        ctx.rule_center_.grant_reflect(1, /*source_slot=*/-1, /*source_effect_id=*/999,
                                       /*target=*/1, /*rounds=*/3);
        CHECK(ctx.rule_center_.has_round_type(1), "T1 前置：对手处于回合类效果");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 冥刃断灵
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_level(0, 1), 1, "T1 吸取搬入：自身特攻+1");
        CHECK_EQ(ctx.ability_level(1, 1), 0, "T1 吸取搬出：对手特攻清零");
        CHECK(!ctx.rule_center_.has_round_type(1), "T1 867（已有）：回合类效果被消除");
        // R2：MOVE_RIGHT 时两个窗口 tick 同刻执行（1123 加自身、867 减对手）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        // R2 仍用冥刃断灵（先3）→ 基础先制 +3 与窗口 +2 同刻累加 = 5
        CHECK_EQ(ctx.ws.preemptive_level[0], 5,
                 "T1 核心：基础先制3 + 吸取成功窗口(+2) —— 下1回合自身所有技能先制+2");
        CHECK_EQ(ctx.ws.preemptive_level[1], -2,
                 "T1 同刻并存：867 的『对手先制-2』窗口互不覆盖");
        b.drive();
        std::cout << "  [T1] 1123 吸取→先制窗口 通过\n";
    }

    //========== T2 982：概率 = 40 + 10×提升种数 ==========
    {
        // 6 种提升 → 100% 短路必发（不掷点，确定性）
        {
            ScenarioBattle b(scenario::make_party(mozun({kXiexin, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                REQUIRE(stat_change(&ctx, 0, i, 1) == StatChangeResult::SUCCESS);
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 邪心黯天决
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFatigue),
                  "T2 核心：6 种提升 → 40+60=100% → 疲惫必落地");
            std::cout << "  [T2a] 982 满提升 100% 路径 通过\n";
        }
        // 无提升 → 基础 40%（种子分支）
        {
            sbs_srand(17);   // 经验调种：rand#1 < 40
            ScenarioBattle b(scenario::make_party(mozun({kXiexin, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFatigue),
                  "T2 基础档：无提升 → 40% 掷点（种子触发分支）");
            std::cout << "  [T2b] 982 基础 40% 分支 通过\n";
        }
    }

    //========== T3 1367：随机 2 种异常 / 全免 → "未触发"封属补偿 ==========
    {
        // 分支 A：对手干净 → 恰 2 条异常落地（不重复挑选、全落地）
        {
            ScenarioBattle b(scenario::make_party(mozun({kTunshi, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 混沌吞噬
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(count_active_anomalies(ctx, 1), 2,
                     "T3 核心：随机附加 2 种异常状态（恰 2 条活跃）");
            CHECK(!has_seal_entry(ctx, 1367, 1), "T3 有落地 → 不触发补偿封属");
            std::cout << "  [T3a] 1367 随机 2 异常 通过\n";
        }
        // 分支 B：对手全免异常 → landed==0 → "未触发"→ 2 回合封属票
        {
            ScenarioBattle b(scenario::make_party(mozun({kTunshi, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*mask=*/0,
                               /*duration_rounds=*/9, /*source_id=*/9993,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/9993);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(count_active_anomalies(ctx, 1), 0, "T3b 前置：异常全被免疫");
            CHECK(has_seal_entry(ctx, 1367, 1),
                  "T3b 核心：未触发（landed==0）→ 令对手属性技能无效 2 回合");
            std::cout << "  [T3b] 1367 未触发补偿封属 通过\n";
        }
    }

    //========== T4 938+731：先手翻倍全属性降 + 致命一击 → 下回合必暴 ==========
    {
        {
            sbs_srand(23);
            ElfPet attacker = mozun({kWuying, kHarmless, kHarmless, kHarmless, kHarmless});
            scenario::set_crit_sixteenths(attacker, 16);   // 必暴（rate=100 不掷点）
            ScenarioBattle b(scenario::make_party(attacker),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 无影禁锢
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_level(1, i), -2,
                         "T4 核心：全属性-1、先出手效果翻倍 → 各槽 -2（含命中）");
            }
            CHECK(ctx.crit_happened[0], "T4 前提：本回合打出致命一击");
            // R2：ROUND_START tick 重写 must_crit（ws 每回合 reset → 窗口每回合重写）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_ROUND_START);
            ctx.debug_step_mode = false;
            CHECK(ctx.ws.must_crit[0], "T4 核心：下1回合其它攻击技能必定致命一击");
            b.drive();
            std::cout << "  [T4a] 938 翻倍+731 必暴窗口 通过\n";
        }
        // 对照：本回合没暴击 → 下回合不置位 must_crit
        {
            ScenarioBattle b(scenario::make_party(mozun({kWuying, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.crit_happened[0], "T4b 前置：暴击率 0 → 无暴击");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_ROUND_START);
            ctx.debug_step_mode = false;
            CHECK(!ctx.ws.must_crit[0], "T4b 对照：无致命一击 → 下回合不置位必暴");
            b.drive();
            std::cout << "  [T4b] 731 无暴击对照 通过\n";
        }
    }

    std::cout << "scenario_146 all passed" << std::endl;
    return 0;
}
