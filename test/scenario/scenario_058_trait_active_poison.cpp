// 场景 058：通用特性·主动异常特性（静电/颤栗/火热/极寒，"主动毒"）。
//
// 口径来源：赛学必修6《精灵特性》（reference/scrape/out/all_posts_final.jsonl）：
//   - "自身的物理攻击或者特殊攻击有 n% 几率使对方 m 异常"；静电/颤栗=物理触发（Eid 66 家族）、
//     火热/极寒=特殊触发（Eid 67 家族）；0-5 星实际概率 args[0] = 3/4/5/6/7/8 百分点
//     （测试构造 args[0]=100 保确定性）；赋予回合随机 2~3；"需要技能命中才可以触发"。
//   - 时点：BATTLE_*_ON_SKILL_HIT（命中判定后、技能效果结算前）；被盔（SKILL_INVALID）
//     不计为命中 → 不触发（必修3）。
//   - 通道：本体条目走 apply_anomaly_ancient（主动毒：无视 Modern 层免疫/弹控/抗性/转化）。
//   - **复制条目不经特性节点**（2026-09-16 游戏实测 + 用户架构拍板）：复制的毒施加走
//     **现代异常效果**（会被现代弹控/抗性响应），已不是主动毒 → 天女魂印自己注册效果到
//     本时点调 apply_anomaly；特性钩子只认本体槽（见 trait_state.h 架构拍板注释）。
//
// 验证点：
//   P1 静电命中施加：物攻命中 → 对手麻痹（2~3 回合）；args[0]=100 确定性触发
//   P2 古早通道：对手挂着 Modern 层免疫票仍被施加（穿透）、且**不消耗**该票
//   P3 对手 Ancient 层票（48 式）：施加被挡
//   P4 物/特类别门：火热（特殊特性）+ 物攻 → 不触发；特攻 → 烧伤
//   P5 属性技能门：静电 + 属性技能 → 不触发
//   P6 miss 门：命中失败 → 不触发（闪避拉满）
//   P7 复制条目不经特性节点（只认本体槽）
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <effects/trait_state.h>
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

// 攻方：args[0]=100 → 100% 触发（真实概率 3~8 不可测）。
ElfPet poison_attacker(const char* trait_name, int status_code) {
    return scenario::make_pet(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              200, 100, 400,
                              {}, scenario::make_trait(trait_name, 5, {100, status_code}));
}
ElfPet special_poison_attacker(const char* trait_name, int status_code) {
    return scenario::make_pet(0, "攻方", {kShuiQiang, kShuiQiang, kShuiQiang, kShuiQiang, kShuiQiang},
                              200, 100, 400,
                              {}, scenario::make_trait(trait_name, 5, {100, status_code}));
}
ElfPet plain_defender() {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, 400);
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

    //========== P1 静电命中施加：物攻 → 对手麻痹（2~3 回合）==========
    {
        ScenarioBattle b({scenario::make_party(poison_attacker("静电", 0))},
                         {scenario::make_party(plain_defender())});
        b.start();
        strike(b);
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "P1 静电应令守方麻痹");
        const int end = ctx.get_abnormal_status_end_round(1, paralysis);
        CHECK(end - 1 == 2 || end - 1 == 3, "P1 赋予回合应为 2~3（施加时 roundCount=1）");
        std::cout << "P1 PASS 静电命中施加麻痹（2~3 回合）\n";
    }

    //========== P2 古早通道：Modern 层免疫票被穿透且不消耗 ==========
    {
        ScenarioBattle b({scenario::make_party(poison_attacker("静电", 0))},
                         {scenario::make_party(plain_defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, /*source_id=*/993,
                           false, EffectScope::ON_STAGE, /*counts=*/1, /*source_effect_id=*/993,
                           ImmunityTier::Modern);
        strike(b);
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "P2 主动毒应穿透 Modern 层免疫");
        CHECK(ctx.is_immune_effect(1, ImmunityType::ANOMALY, ctx.currentState, paralysis),
              "P2 穿透不应消耗 Modern 次数票");
        std::cout << "P2 PASS 主动毒穿透 Modern 免疫且不消耗\n";
    }

    //========== P3 Ancient 层票挡主动毒 ==========
    {
        ScenarioBattle b({scenario::make_party(poison_attacker("静电", 0))},
                         {scenario::make_party(plain_defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 3, /*source_id=*/994,
                           false, EffectScope::ON_STAGE, 0, /*source_effect_id=*/994,
                           ImmunityTier::Ancient);
        strike(b);
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "P3 Ancient 层票应挡住主动毒");
        std::cout << "P3 PASS Ancient 层免疫挡主动毒\n";
    }

    //========== P4 火（特）物（理）类别门：火热+物攻不触发；火热+特攻烧伤 ==========
    {
        ScenarioBattle b({scenario::make_party(poison_attacker("火热", 2))},
                         {scenario::make_party(plain_defender())});
        b.start();
        strike(b);   // 火热（特殊特性）打物攻 → 不触发
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, burn), "P4 火热+物攻不应触发");
        std::cout << "P4a PASS 火热不吃物攻\n";
    }
    {
        ScenarioBattle b({scenario::make_party(special_poison_attacker("火热", 2))},
                         {scenario::make_party(plain_defender())});
        b.start();
        strike(b);   // 火热（特殊特性）打特攻 → 烧伤
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, burn), "P4 火热+特攻应令守方烧伤");
        std::cout << "P4b PASS 火热吃特攻→烧伤\n";
    }

    //========== P5 属性技能门：静电+属性技能不触发 ==========
    {
        ElfPet atk = scenario::make_pet(0, "攻方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        200, 100, 400, {},
                                        scenario::make_trait("静电", 5, std::vector<int>{100, 0}));
        ScenarioBattle b({scenario::make_party(atk)}, {scenario::make_party(plain_defender())});
        b.start();
        strike(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, paralysis), "P5 属性技能不应触发静电");
        std::cout << "P5 PASS 属性技能不触发\n";
    }

    //========== P6 miss 门：命中失败不触发 ==========
    {
        ElfPet atk = poison_attacker("静电", 0);
        ElfPet dodge_def = plain_defender();
        dodge_def.soulMark = scenario::make_dodge_soul_mark();  // 守方闪避拉满 → 攻方 miss
        ScenarioBattle b({scenario::make_party(atk)}, {scenario::make_party(dodge_def)});
        b.start();
        strike(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, paralysis), "P6 miss 不应触发静电");
        std::cout << "P6 PASS miss 不触发\n";
    }

    //========== P7 复制条目不经特性节点（2026-09-16 架构拍板）==========
    {
        // 游戏实测：天女复制的毒虽然取消类别要求，但施加走**现代异常效果**（会被弹控/抗性
        // 响应），不再是主动毒 → 不属于"特性施加"。落地方案（用户拍板）：天女魂印把毒拷到
        // pet 容器 + 自己注册效果到 ON_SKILL_HIT 调 apply_anomaly；**特性钩子只认本体槽**。
        // 因此挂在 context 槽上的复制毒在这里不触发（天女魂印本体待做）。
        ElfPet atk = special_poison_attacker("无", 0);   // 本体无特性
        ScenarioBattle b({scenario::make_party(atk)}, {scenario::make_party(plain_defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        EffectiveTrait copied;
        copied.kind = TraitKind::ActivePoisonPhysical;
        copied.idx = 1070;
        copied.star_level = 5;
        copied.args[0] = 100;
        copied.args[1] = 0;
        copied.proc_forced = true;
        ctx.copy_common_trait(0, copied, /*source_owner=*/0);
        strike(b);   // 特攻 + 复制静电：特性钩子不认复制条目 → 什么都不发生
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "P7 复制毒不应经特性节点施加");
        CHECK(!ctx.has_active_abnormal_status(0, paralysis), "P7 攻方也不应有异常");
        std::cout << "P7 PASS 复制条目不经特性节点（天女架构拍板）\n";
    }

    //========== P8 先后顺序 + 同类型异常回合覆盖（用户 2026-09-16 口径）==========
    {
        // 顺序：特性毒先落（ON_SKILL_HIT，技能效果结算之前）→ 技能自身效果后落。
        // 同类型合并：**后落者回合数更大才覆盖**（否则保留原有的）——即"特性附加的异常"
        // 不会被技能的同类型短控顶掉，却被技能的同类型长控刷新。
        // 注：本组只锁"合并规则"这一可观测面；先后顺序由时点结构保证（钩子在 SKILL_EFFECT
        //     桶之前跑），因长者覆盖的语义使两种顺序在最终回合数上不可区分。
        ScenarioBattle b({scenario::make_party(poison_attacker("静电", 0))},
                         {scenario::make_party(plain_defender())});
        b.start();
        strike(b);   // 静电 → 守方麻痹（2~3 回合）
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "P8 前提：特性毒已落");
        const int trait_end = ctx.get_abnormal_status_end_round(1, paralysis);
        const int trait_remaining = trait_end - ctx.roundCount;
        CHECK(trait_remaining >= 1, "P8 前提：特性毒仍有剩余回合");

        // ① 后落的同类型异常回合数**更大** → 覆盖
        const int longer = trait_remaining + 2;
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, longer, 0),
                 ApplyAnomalyResult::DURATION_EXTENDED, "P8 更长应覆盖（返回 DURATION_EXTENDED）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, paralysis) - ctx.roundCount, longer,
                 "P8 覆盖后回合数应为后落者");

        // ② 后落的同类型异常回合数**更小** → 不覆盖（保留原回合数）
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 1, 0),
                 ApplyAnomalyResult::SUCCESS, "P8 更短不覆盖（返回 SUCCESS 非 EXTENDED）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, paralysis) - ctx.roundCount, longer,
                 "P8 更短不应改写回合数");

        // ③ 跨通道同样遵守：古早施加的同类型短控也不顶掉现代长控
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 1, 0),
                 ApplyAnomalyResult::SUCCESS, "P8 古早短控不覆盖");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, paralysis) - ctx.roundCount, longer,
                 "P8 古早短控不应改写回合数");
        std::cout << "P8 PASS 先后顺序 + 同类型回合长者覆盖\n";
    }

    std::cout << "\n场景 058 全部通过：通用特性·主动毒（静电/颤栗/火热/极寒）✓" << std::endl;
    return 0;
}
