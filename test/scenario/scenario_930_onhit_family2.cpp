// 场景 930 —— J9 受击响应族二（4 宏，moves_lib/j_onhit_family2.cpp）
//（2026-10-01，worktree ../agents/j9；号段 930-934 → T930-T934 五块）
//
// 断言口径：
//   - 主干 chance=100 载体必施加（无随机精确断言，J3/F6 同款）；
//   - "未触发"强制手段 = 对手挂 ANOMALY 窗口（setup 直授 grant_immunity，916/1255
//     口径"未触发含被挡各档"）→ 兜底全部确定性触发；
//   - 1322 窗口期数 = 兜底阶梯（异常窗常驻 → 窗内每刀 -1、出窗不动）；
//   - 1200 兜底探针 = 对手 ROUND_START 计数节点（886 同款构造）：R1 受击被 break
//     清掉 → R2 无 tick（计数停 1）；break 目标错挂到己方则计数继续、必红；
//   - 1594 主干 {1} 全库均 50 → 16 场统计断言（F6 850 同款带）；
//   - 攻击噪声消音：对手 atk=1、双方 accuracy=100（自方叠 must_hit）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对，全为属性技 power=0）──
constexpr int kPaDa    = 10004;   // 拍打：物40（对手攻击/填充）
constexpr int kLeiTing = 26450;   // 雷霆魔剑[191,1322,837]        1322=[4,100,0,1]
constexpr int kBeiFeng = 26468;   // 北风吹雪[191,47,1322]         1322=[3,100,0,1]
constexpr int kFuChou  = 25607;   // 复仇之欲[532,1200,843]        1200=[4,100,6]
constexpr int kHunXue  = 27229;   // 混沌血源[191,1594,767,1121]   1594=[3,50,1,100,1]
constexpr int kCiHan   = 27244;   // 淬寒铸体[191,47,1599,1475]    1599=[4,100,2,100,5]
constexpr int kAnLei   = 27270;   // 暗雷聚涌[191,677,1599,693]    1599=[4,100,6,100,0]

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// setup 直授：ANOMALY 窗口（counts=0 永续）——"未触发"强制器（J3 同款）。
// ⚠️ 只适用于兜底为**非异常**的宏（1322/1200/1594）：窗口连兜底异常一起挡。
void grant_anomaly_window(BattleContext& ctx, int side, int source_id) {
    ctx.grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, source_id, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, /*counts=*/0, source_id);
}

// setup 直授：ANOMALY 次免票（counts=1）——1599 专用：挡下主干并**耗尽次数**，
// 兜底异常随即能落地（J3 T904.g/R3 次数耗尽同款语义）。
void grant_foe_anomaly_counts(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9304, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, counts, /*source_effect_id=*/9304);
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

bool levels_all_eq(BattleContext& ctx, int side, int v) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

// 1200 兜底探针：对手侧 ROUND_START 计数节点（886 同款构造；有限窗=真窗口形态，
// break 可清）。R1 受击被 break → R2 无 tick，计数停在 1。
int g_break_probe_ticks = 0;
EffectResult break_probe_tick(BattleContext*, const EffectArgs&) {
    ++g_break_probe_ticks;
    return EffectResult::kOk;
}

void register_foe_round_probe(BattleContext* ctx) {
    Effect e(93001, 0, /*owner=*/1, /*left_round=*/-1, EffectArgs(), &break_probe_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, /*owner=*/1,
                                                 /*duration=*/5, ctx->roundCount);
    ce->source_id_ = 93001;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T930 1322 主干：chance=100 必施加，兜底不触发（雷霆魔剑 [4,100,0,1]）══
    {
        ScenarioBattle b(make_party(self_pet({kLeiTing, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 雷霆魔剑
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手拍打 → 受击
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kParalysis),
              "T930 1322 主干：受击后 100% 令对手麻痹");
        CHECK(levels_all_eq(ctx, 1, 0),
              "T930 1322 兜底未触发：对手全属性不动");
    }

    // ══ T931 1322 未触发→全属性-1 + {0} 回合窗口阶梯（北风吹雪 [3,100,0,1]）══
    //   对手挂 ANOMALY 永续窗 → 窗内每刀 anomaly 被挡 → 兜底 -1：R1 -1 / R2 -2 / R3 -3
    //   / R4（窗口 rounds=3 已过期）不再降。
    {
        ScenarioBattle b(make_party(self_pet({kBeiFeng, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_anomaly_window(ctx, 1, 9301);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 北风吹雪
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis), "T931 前置：主干被异常窗挡下");
        CHECK(levels_all_eq(ctx, 1, -1), "T931 1322 兜底：R1 首刀对手全属性-1");
        for (int r = 2; r <= 3; ++r) {   // R2/R3 仍在窗口内
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(levels_all_eq(ctx, 1, -r),
                  "T931 1322 窗口内：R" + std::to_string(r) + " 阶梯至全属性-" +
                      std::to_string(r));
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R4：窗口已过期
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(levels_all_eq(ctx, 1, -3), "T931 1322 出窗：R4 起不再触发");
        std::cout << "  [T931] 1322 未触发兜底 + 窗口阶梯 通过\n";
    }

    // ══ T932 1200 主干 + 未触发→消除对手回合类效果（复仇之欲 [4,100,6]）══
    //   主干：害怕落地（磁暴链条带 570 免攻盔会吃刀，故用无盔载体复仇之欲）。
    //   兜底：异常窗挡害怕 → break_round_effects(攻击者)；探针=对手 ROUND_START
    //   计数节点，R1 被清 → R2 无 tick（计数停 1；清错边则计数 2 必红）。
    {
        ScenarioBattle b(make_party(self_pet({kFuChou, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T932.a 1200 主干：100% 害怕落地");
    }
    {
        g_break_probe_ticks = 0;
        ScenarioBattle b(make_party(self_pet({kFuChou, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        register_foe_round_probe(&ctx);
        grant_anomaly_window(ctx, 1, 9302);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用技+挨刀
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(g_break_probe_ticks, 1, "T932.b 前置：探针 R1 ROUND_START 已 tick 一次");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T932.b 前置：害怕被异常窗挡下");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R2：探针已被 break 清除
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(g_break_probe_ticks, 1,
                 "T932.b 1200 兜底：对手回合类效果被消除（R2 无 ROUND_START tick）");
        std::cout << "  [T932] 1200 主干 + 未触发消回 通过\n";
    }

    // ══ T933 1594 主干/兜底统计（混沌血源 [3,50,1,100,1]，{1} 全库均 50）══
    //   16 场：受击 → 50% 自身全属性+1；未掷中 → 50% 对手全属性-1。双向都应出现。
    {
        constexpr int kTrials = 16;
        int main_up = 0, fallback_down = 0;
        SbsRng trial_seeds(20261004);
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(make_party(self_pet({kHunXue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.ctx().seed_rng(trial_seeds.next());
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (levels_all_eq(ctx, 0, 1)) {
                ++main_up;
            } else if (levels_all_eq(ctx, 1, -1)) {
                ++fallback_down;
            }
        }
        CHECK(main_up >= 1, "T933 1594 主干：50% 自身全属性+1 应至少命中一次");
        CHECK(main_up <= kTrials - 1, "T933 1594 主干：50% 不应全命中");
        CHECK(fallback_down >= 1, "T933 1594 兜底：未掷中→50% 对手全属性-1 应至少出现");
        std::cout << "  [T933] 1594 统计断言 通过 (主干+1=" << main_up << "/"
                  << kTrials << " 兜底-1=" << fallback_down << ")\n";
    }

    // ══ T934 1599 主干 + 兜底二次申报 ══
    //   主干：淬寒铸体 [4,100,2,100,5] → 烧伤落地、冻伤（兜底）不接。
    //   兜底：暗雷聚涌 [4,100,6,100,0] + 异常窗 → 害怕被挡 → 麻痹落地。
    {
        ScenarioBattle b(make_party(self_pet({kCiHan, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T934.a 1599 主干：100% 烧伤落地");
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T934.a 1599 兜底未触发：冻伤不接");
    }
    {
        ScenarioBattle b(make_party(self_pet({kAnLei, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_counts(ctx, 1);   // 只挡主干一刀：次数耗尽后兜底可落地
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T934.b 前置：害怕被次免票挡下");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis),
              "T934.b 1599 兜底：未触发→100% 麻痹落地");
        std::cout << "  [T934] 1599 主干 + 兜底二次申报 通过\n";
    }

    std::cout << "scenario_930 PASS (J9 受击响应族二 1322/1200/1594/1599)" << std::endl;
    return 0;
}
