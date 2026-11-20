// 场景 031：次数型免伤接入伤害结算管线（BLOCK 类别 + BLOCK 阶段）+ "挡伤失效"掩码交互。
//
// 背景（怒涛·沧岚 蚀砚之泪≥4滴）：官方"使对手挡伤失效"只废**归零类**挡伤
// （免伤/抵挡伤害/弹伤/受高伤转化），**减伤不被无视**（"减伤100%、点数减伤全都是
// 不能被无视的效果"）。故 DamageEffectCategory 拆出 BLOCK：
//   MITIGATE(减伤) 不可被无视 ／ BLOCK(挡伤归零)+DETECT(受高伤触发) 可被无视。
//
// 次数型免伤（"免疫下N次攻击伤害"，谱尼虚无 / 沧岚"抵挡下1次攻击"）是挡伤一族，
// 必须走管线而非 inline 判断，才能吃同一道抑制门 —— 被压制时**既不触发也不扣次数**。
//
// 验证点：
//   Q1 未授予免疫 → 红伤正常
//   Q2 次数型免伤 counts=1 → 第1次攻击伤害 0，第2次正常（次数被消费）
//   Q3 counts=2 → 前两次都挡下，第三次正常
//   Q4 掩码(BLOCK|DETECT)置位 → 挡伤不触发、伤害正常，且**次数未被消费**（清掩码后仍能挡）
//   Q5 掩码下减伤(MITIGATE)仍生效（减伤不可被无视）
//   Q6 BLOCK 阶段归零 → DETECT 阶段的"受高伤"判定（final>阈值）不成立
//   Q7~Q9 异常侧同一条通道：次数型免异常挡下并消费 / 与窗口类免控并存时**照常消耗** /
//        被弹回的异常**不消耗**（官方 idx=418/375，反直觉）
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;  // 撞击：普通(8) power 35

// 攻击方速度 300/防守方 100 → pet0 恒定先手，攻击发生在 BATTLE_FIRST_ATTACK_DAMAGE。
ElfPet make_atk() {
    return scenario::make_pet(0, "攻方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                          kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/300, /*atk=*/100, /*hp=*/800);
}
ElfPet make_def() {
    return scenario::make_pet(1, "守方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                          kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/5000);
}

// 给守方(1)授予"免疫下 counts 次受到的攻击伤害"（覆盖两个攻击伤害时点）。
void grant_count_immune(scenario::ScenarioBattle& battle, int counts) {
    const uint64_t kAttackDamageCoverage = coverage_union(
        {State::BATTLE_FIRST_ATTACK_DAMAGE, State::BATTLE_SECOND_ATTACK_DAMAGE});
    battle.ctx().grant_immunity(
        /*owner=*/1, ImmunityType::DAMAGE, kAttackDamageCoverage,
        /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/counts);
}

// 跑一回合（双方都出 1 号位技能），返回守方本回合掉血。
int attack_round(scenario::ScenarioBattle& battle) {
    const int hp_before = battle.ctx().getPet(1).hp;
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    return hp_before - battle.ctx().getPet(1).hp;
}

int damage_guard_suppress_bits() {
    return (1 << static_cast<int>(DamageEffectCategory::BLOCK))
         | (1 << static_cast<int>(DamageEffectCategory::DETECT));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== Q1 基线：未授予免疫 → 红伤正常 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        b.start();
        const int dmg = attack_round(b);
        CHECK(dmg > 0, "Q1) 未授予次数免伤 → 红伤应正常作用");
        std::cout << "  [Q1] 基线红伤=" << dmg << std::endl;
    }

    //========== Q2 counts=1：挡下第1次，第2次正常（次数被消费）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        b.start();
        grant_count_immune(b, /*counts=*/1);
        const int dmg1 = attack_round(b);
        const int dmg2 = attack_round(b);
        CHECK_EQ(dmg1, 0, "Q2) 第1次攻击应被次数免伤完全挡下");
        CHECK(dmg2 > 0, "Q2) 次数已消费 → 第2次攻击应正常造成伤害");
        std::cout << "  [Q2] counts=1: 第1次=" << dmg1 << " 第2次=" << dmg2
                  << "（次数被消费）" << std::endl;
    }

    //========== Q3 counts=2：前两次都挡下，第三次正常 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        b.start();
        grant_count_immune(b, /*counts=*/2);
        const int dmg1 = attack_round(b);
        const int dmg2 = attack_round(b);
        const int dmg3 = attack_round(b);
        CHECK_EQ(dmg1, 0, "Q3) 第1次应被挡下");
        CHECK_EQ(dmg2, 0, "Q3) 第2次应被挡下（counts=2）");
        CHECK(dmg3 > 0, "Q3) 两次用尽 → 第3次正常");
        std::cout << "  [Q3] counts=2: " << dmg1 << " / " << dmg2 << " / " << dmg3
                  << "（前两次挡下）" << std::endl;
    }

    //========== Q4 挡伤失效掩码：不触发、不扣次数 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        b.start();
        grant_count_immune(b, /*counts=*/1);
        ElfPet& def = b.ctx().getPet(1);
        def.damage_suppress_mask |= damage_guard_suppress_bits();  // 蚀砚之泪≥4滴

        const int dmg_suppressed = attack_round(b);
        CHECK(dmg_suppressed > 0, "Q4) 挡伤失效掩码置位 → 次数免伤不应触发，伤害正常");

        def.damage_suppress_mask &= ~damage_guard_suppress_bits();  // 压制解除
        const int dmg_after = attack_round(b);
        CHECK_EQ(dmg_after, 0,
                 "Q4) 被压制期间**不得消费次数** → 解除压制后仍应挡下");
        std::cout << "  [Q4] 压制中=" << dmg_suppressed << " 解除后=" << dmg_after
                  << "（次数未被消费）" << std::endl;
    }

    //========== Q5 掩码下减伤仍生效（减伤不可被"挡伤失效"无视）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).damage_suppress_mask |= damage_guard_suppress_bits();
        ctx.damage_reduce_add[1][0] = 25;  // 守方加算减伤 25%
        ctx.resolvedDamage.attackerId = 0;
        ctx.resolvedDamage.defenderId = 1;
        ctx.resolvedDamage.isRed = true;  // ws.reset() 是 memset，isRed 默认被清成 false
        ctx.resolvedDamage.final = 100;
        ctx.damage_pipeline_.run(&ctx, 0, 1);
        CHECK_EQ(ctx.resolvedDamage.final, 75,
                 "Q5) 掩码只废挡伤(BLOCK)不废减伤(MITIGATE)：100 → 75");
        std::cout << "  [Q5] 掩码下减伤: 100 → " << ctx.resolvedDamage.final << std::endl;
    }

    //========== Q6 BLOCK 归零 → DETECT 的"受高伤"判定不成立 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        BattleContext& ctx = b.ctx();
        grant_count_immune(b, /*counts=*/1);
        // 不走 FSM，直接驱动管线：把时点显式设为攻击伤害时点（免伤覆盖率按此匹配，
        // 否则 currentState 停在 GAME_START，覆盖位对不上）。
        ctx.currentState = State::BATTLE_FIRST_ATTACK_DAMAGE;

        int detect_high_damage_seen = 0;
        ctx.register_damage_effect(
            DamagePhase::DETECT, 1, DamageEffectCategory::DETECT,
            [&detect_high_damage_seen](BattleContext* c, int owner) {
                if (c->resolvedDamage.defenderId == owner && c->resolvedDamage.final > 50) {
                    ++detect_high_damage_seen;  // 受高伤触发（回血/弹伤/转化体力一族）
                }
            });

        ctx.resolvedDamage.attackerId = 0;
        ctx.resolvedDamage.defenderId = 1;
        ctx.resolvedDamage.isRed = true;  // ws.reset() 是 memset，isRed 默认被清成 false
        ctx.resolvedDamage.final = 100;
        ctx.damage_pipeline_.run(&ctx, 0, 1);
        CHECK_EQ(ctx.resolvedDamage.final, 0, "Q6) BLOCK 阶段应把伤害归零");
        CHECK_EQ(detect_high_damage_seen, 0,
                 "Q6) 挡伤成立后 DETECT 的受高伤判定读到 0 → 不应触发");
        std::cout << "  [Q6] BLOCK 归零=" << ctx.resolvedDamage.final
                  << " 受高伤触发次数=" << detect_high_damage_seen << std::endl;
    }

    //========== Q7 次数型免异常：挡下并消费 ==========
    {
        constexpr int kBurn = 12;
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/1);

        const auto r1 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(r1 == ApplyAnomalyResult::TARGET_IMMUNE || r1 == ApplyAnomalyResult::REFLECTED,
              "Q7) 第1次异常应被次数型免异常挡下");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "Q7) 第1次异常不应挂上");

        const auto r2 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(r2 == ApplyAnomalyResult::SUCCESS, "Q7) 次数已消费 → 第2次异常应正常命中");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "Q7) 第2次异常应挂上");
        std::cout << "  [Q7] 次数型免异常 counts=1: 第1次挡下、第2次命中（次数被消费）\n";
    }

    //========== Q8 窗口类免控 + 次免并存 → 次免照常消耗（官方反直觉规则）==========
    {
        constexpr int kBurn = 12;
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        BattleContext& ctx = b.ctx();
        // 窗口类免控（5 回合，counts=0）与 次免（1 次）来自**不同来源**，
        // 故各传一个 effect_id —— 否则同类型免疫会互相覆盖（覆盖键的 source 维度）。
        const int window_id = ctx.grant_immunity(
            /*owner=*/1, ImmunityType::ANOMALY, coverage_all(), 0,
            /*duration_rounds=*/5, /*source_id=*/0, /*soul_immunity=*/false,
            EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/70001);
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(), 0,
                           /*duration_rounds=*/0, /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/1, /*source_effect_id=*/70002);

        const auto r1 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "Q8) 窗口免控挡下第1次异常");

        // 窗口免控仍在，但次免已被第1次消耗 → 第2次异常仍被窗口挡下
        const auto r2 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "Q8) 窗口免控仍挡下第2次");

        // 关键断言：次免确实在"窗口免控挡下"的同时被消耗了 → 撤掉窗口后第3次直接命中
        ctx.revoke_immunity(1, window_id);
        const auto r3 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(r3 == ApplyAnomalyResult::SUCCESS,
              "Q8) 次免已在窗口免控挡下时被消耗 → 撤掉窗口后异常命中");
        (void)r1;
        (void)r2;
        std::cout << "  [Q8] 窗口免控并存: 次免照常消耗（官方 idx=418 第2条）\n";
    }

    //========== Q9 被弹回的异常不消耗次免 ==========
    {
        constexpr int kBurn = 12;
        scenario::ScenarioBattle b(scenario::make_party(make_atk()),
                                   scenario::make_party(make_def()));
        BattleContext& ctx = b.ctx();
        // pet1 免疫 + 反弹（弹控 = 免疫 + 反弹，只有免疫成立才会走 reflect()）
        // → 异常弹回施放方 pet0（reflect_depth=1）
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(), 0,
                           /*duration_rounds=*/5, /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/70004);
        ctx.rule_center_.grant_reflect(/*source=*/1, /*slot=*/-1, /*eff=*/70040, /*target=*/1, 0);
        // pet0 有 1 层次免
        ctx.grant_immunity(/*owner=*/0, ImmunityType::ANOMALY, coverage_all(), 0,
                           /*duration_rounds=*/0, /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/1, /*source_effect_id=*/70003);
        ctx.rule_center_.grant_reflect(/*source=*/0, /*slot=*/-1, /*eff=*/70041, /*target=*/0, 0);

        const auto r = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(r == ApplyAnomalyResult::REFLECTED, "Q9) pet1 弹控应把异常弹回 pet0");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "Q9) 弹回 pet0 的异常被其次免挡下");

        // 弹回的异常**不算**"受到异常" → 次免不应被消耗：再来一次仍挡下
        const auto r2 = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        CHECK(r2 == ApplyAnomalyResult::REFLECTED, "Q9) 再次弹回");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn),
              "Q9) 被弹回的异常不消耗次免 → 第2次弹回仍被挡下");
        std::cout << "  [Q9] 被弹回的异常不消耗次免（官方 idx=375）\n";
    }

    std::cout << "\n场景 031 通过：次数型免伤走 BLOCK 阶段/类别，"
                 "被\"挡伤失效\"压制时不触发也不扣次数；减伤不受压制；"
                 "异常侧次数免疫挡下即消费（含窗口并存时照常消耗、弹回不消耗）。\n";
    return 0;
}
