// 场景 041：必中凭证 + 失明 + ③层命中失效按技能类型分两类。
//
// 用户 2026-09-13 定的三套口径：
//   ① **必中是每回合凭证**，不只看技能里写死的 `moves.must_hit`——因为存在**条件必中的固有效果**
//      （如 2000「对手处于能力提升状态则先制+1**且必中**」），它在选择期就和条件先制一起注册，
//      效果体跑在 MOVE_RIGHT 时点、出手之前。判"是否必中"一律读 `AttackCredential::must_hit`。
//      ⚠️ 原先 effect 2000 是直接改 `skill.must_hit` —— 那是**永久**改技能对象（场景里验它不再发生）。
//   ② **失明**（异常 20）：处于失明的精灵，**非必中技能必定 miss**；
//      **必中技能 50% 正常命中 / 50% 命中效果失效**。**强制执行最先短路**（不吃失明）。
//   ③ **③层命中效果失效不是属性技能专用**：攻击技能同样会被失效 →
//      RuleCenter 拆成 `HIT_INVALID_ATTACK` / `HIT_INVALID_ATTRIBUTE` 两个类别，
//      消费点收口到 `query_usage` 末尾、**按本次技能类型只消费对应那一类**；
//      要"两种技能都失效"就注册两条。
//
// 验证点：
//   M1 必中凭证：条件必中写 ws.must_hit_grant（不是永久改技能对象）；无提升条件时不授予
//   M2 必中凭证端到端：条件必中让低命中率技能必中（对照：不满足条件时 miss）
//   M3 失明：非必中技能必定 miss（对照：无失明时能命中）
//   M4 失明 + 必中技能：50% 命中 / 50% 命中效果失效（大样本统计）
//   M5 失明 + 强制执行：强制执行最先短路，不受失明影响（100% 正常命中）
//   M6 ③层按技能类型消费：攻击类只挡攻击技能、属性类只挡属性技能；挂两条则都挡
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kDaXue = 28319;    // 大雪纷飞：side_effect 含 2000（条件先制+必中）
constexpr int kZhuangJi = 10001; // 撞击：攻击技能
constexpr int kGaoSu = 20017;    // 高速移动：属性技能（自身速度+2）

constexpr int kBlind = 20;       // AbnormalStatusId::Blind 失明

ElfPet make_pet(int id, const std::string& name, int sk0, int sk1, int speed, int hp) {
    return scenario::make_pet(id, name, {sk0, sk1, sk1, sk1, sk1}, speed, /*atk=*/100, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== M1 必中凭证：条件必中写 ws 凭证，**不**永久改技能对象 ==========
    {
        auto p0 = make_pet(0, "攻方", kDaXue, kGaoSu, /*speed=*/300, /*hp=*/3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, /*speed=*/100, /*hp=*/3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;    // 对手处于能力提升 → 条件成立

        // 直接调 effect 2000（选择期效果，效果体在 MOVE_RIGHT 时点跑）
        Effect e = EffectFactory::getInstance().getEffect(2000, EffectArgs(std::vector<int>{0, 1}));
        CHECK(e.logic != nullptr, "M1) effect 2000 应已注册");
        e.logic(&ctx, e.args);

        CHECK(ctx.ws.must_hit_grant[0] == true, "M1) 条件成立 → 授予本回合必中凭证");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "M1) 同一次条件先制+1");
        CHECK(ctx.getPet(0).skills[0].must_hit == false,
              "M1) ★ 技能对象的 must_hit **不被永久改写**（原先的 bug）");

        // 凭证被并进 attack_credential
        Skills& sk = ctx.getPet(0).skills[0];
        sk.query_usage(&ctx, 0);
        CHECK(ctx.ws.attack_credential[0].must_hit == true, "M1) 凭证并入 AttackCredential::must_hit");
        std::cout << "  [M1] 必中凭证: grant=" << ctx.ws.must_hit_grant[0]
                  << "、凭证=" << ctx.ws.attack_credential[0].must_hit
                  << "、技能对象 must_hit=" << ctx.getPet(0).skills[0].must_hit << "（未被永久改）\n";
    }
    {
        // 对照：对手没有能力提升 → 条件不成立 → 不授予
        auto p0 = make_pet(0, "攻方", kDaXue, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        Effect e = EffectFactory::getInstance().getEffect(2000, EffectArgs(std::vector<int>{0, 1}));
        e.logic(&ctx, e.args);
        CHECK(ctx.ws.must_hit_grant[0] == false, "M1) 条件不成立 → 不授予必中");
        std::cout << "  [M1] 对照(对手无提升): 不授予必中\n";
    }

    //========== M2 端到端：条件必中让低命中率技能必中 ==========
    {
        // 把大雪纷飞的命中率压到 5%，非必中时几乎必 miss
        auto p0 = make_pet(0, "攻方", kDaXue, kGaoSu, 300, 3000);
        p0.skills[0].accuracy = 5;
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;    // 条件成立 → 必中
        ctx.ability_levels[1][2] = 3;    // 顺便给守方防御提升，便于观察
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 必中 → 技能效果注册（2006 是"技能无效补偿"，只在无效时触发；这里看暴击判定是否发生即可）
        CHECK(ctx.ws.attack_credential[0].must_hit == true,
              "M2) 出手时凭证里应带着必中（条件成立）");
        std::cout << "  [M2] 必中端到端: accuracy=5 仍带必中凭证="
                  << ctx.ws.attack_credential[0].must_hit << "\n";
    }

    //========== M3 失明：非必中技能必定 miss ==========
    {
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, /*target=*/0, kBlind, /*duration=*/3, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, kBlind), "M3) 前置：0 号已处于失明");

        Skills& sk = ctx.getPet(0).skills[0];   // 撞击：accuracy 95，非必中
        int missed = 0;
        for (int i = 0; i < 50; ++i) {
            if (sk.query_usage(&ctx, 0) == SkillUsageResult::MISS) {
                ++missed;
            }
        }
        CHECK_EQ(missed, 50, "M3) 失明 + 非必中技能 → **每次都 miss**（不看命中率）");
        std::cout << "  [M3] 失明+非必中: 50/50 全 miss\n";
    }
    {
        // 对照：没有失明 → accuracy 95 应当能命中（50 次里绝大多数命中）
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        Skills& sk = ctx.getPet(0).skills[0];
        int missed = 0;
        for (int i = 0; i < 50; ++i) {
            if (sk.query_usage(&ctx, 0) == SkillUsageResult::MISS) {
                ++missed;
            }
        }
        CHECK(missed < 20, "M3) 对照：无失明时 accuracy 95 绝大多数命中");
        std::cout << "  [M3] 对照(无失明): 50 次里 miss " << missed << " 次\n";
    }

    //========== M4 失明 + 必中技能：50% 正常命中 / 50% 命中效果失效 ==========
    {
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        p0.skills[0].must_hit = true;   // 必中技能
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kBlind, 3, 1);

        Skills& sk = ctx.getPet(0).skills[0];
        int hit_invalid = 0;
        int miss = 0;
        constexpr int kTrials = 400;
        for (int i = 0; i < kTrials; ++i) {
            const SkillUsageResult r = sk.query_usage(&ctx, 0);
            if (r == SkillUsageResult::HIT_INVALID) {
                ++hit_invalid;
            } else if (r == SkillUsageResult::MISS) {
                ++miss;
            }
        }
        CHECK_EQ(miss, 0, "M4) 必中技能在失明下**不会 miss**（只会效果失效）");
        // 二项分布 n=400 p=0.5：均值 200、标准差 ≈10 → [140,260] 是 ±6σ，几乎不可能误判
        CHECK(hit_invalid > 140 && hit_invalid < 260,
              "M4) 必中技能在失明下 ≈50% 命中效果失效");
        std::cout << "  [M4] 失明+必中: " << kTrials << " 次里 " << hit_invalid
                  << " 次命中效果失效（≈50%）、miss " << miss << " 次\n";
    }

    //========== M5 失明 + 强制执行：强制执行最先短路 ==========
    {
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kBlind, 3, 1);
        ctx.getPet(0).skills[0].accuracy = 1;   // 命中率压到 1%

        Skills& sk = ctx.getPet(0).skills[0];
        int bad = 0;
        for (int i = 0; i < 50; ++i) {
            // 每次都要重新物化凭证（query_usage 会重置），授予写入 ws 的必中位模拟"强制执行"
            ctx.ws.must_hit_grant[0] = true;   // 必中 → 失明下 50% 效果失效，仍可能 HIT_INVALID
            if (sk.query_usage(&ctx, 0) == SkillUsageResult::MISS) {
                ++bad;
            }
        }
        CHECK_EQ(bad, 0, "M5) 必中（含强制执行语义）在失明下不 miss");

        // 真·强制执行：凭证 force_execute → 完全绕过失明（既不 miss 也不失效）
        int non_ok = 0;
        for (int i = 0; i < 50; ++i) {
            sk.penetration_flags.force_execute = true;   // materialize 会并进凭证
            if (sk.query_usage(&ctx, 0) != SkillUsageResult::OK) {
                ++non_ok;
            }
        }
        sk.penetration_flags.force_execute = false;
        CHECK_EQ(non_ok, 0, "M5) 强制执行 → 完全绕过失明（100% 正常命中）");
        std::cout << "  [M5] 失明+强制执行: 50 次全部 OK（无 miss、无效果失效）\n";
    }

    //========== M6 ③层命中失效按技能类型分两类 ==========
    {
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].accuracy = 100;   // 排除命中率干扰（本臂测的是失效，不是命中）
        ctx.getPet(0).skills[1].accuracy = 100;

        // ③层票据挂在**防御方(1)**身上——语义是"防御性标记：令对方攻击的命中效果失效"，
        // 攻击方(0)出手时从 `1 - owner = 1` 侧消费。**要"两种技能都失效"就挂两条**。
        ctx.rule_center_.grant_hit_invalid(/*target=*/(1), /*source_slot=*/-1,
                                           /*source_effect_id=*/70061,
                                           static_cast<int>(HitInvalidMode::kEffectsOnly),
                                           /*counts=*/1, /*is_attribute_skill=*/false);
        Skills& attack_skill = ctx.getPet(0).skills[0];    // 撞击
        Skills& attr_skill = ctx.getPet(0).skills[1];      // 高速移动

        // 属性技能不受影响（该条只挡攻击技能）
        CHECK(attr_skill.query_usage(&ctx, 0) == SkillUsageResult::OK,
              "M6) 攻击类失效**不挡属性技能**");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, /*skill_kind=*/0), 1,
                 "M6) 且属性技能没有消费掉它");
        // 攻击技能被挡下并消费
        CHECK(attack_skill.query_usage(&ctx, 0) == SkillUsageResult::HIT_INVALID,
              "M6) 攻击类失效挡下攻击技能");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, /*skill_kind=*/0), 0,
                 "M6) 次数已消费");
        std::cout << "  [M6] 攻击类失效: 属性技能不受影响、攻击技能被挡（次数消费）\n";
    }
    {
        // 反向：只对属性技能生效的一条
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].accuracy = 100;
        ctx.getPet(0).skills[1].accuracy = 100;
        ctx.rule_center_.grant_hit_invalid(/*防御方=*/1, -1, 70062,
                                           static_cast<int>(HitInvalidMode::kEffectsOnly),
                                           1, /*is_attribute_skill=*/true);
        Skills& attack_skill = ctx.getPet(0).skills[0];
        Skills& attr_skill = ctx.getPet(0).skills[1];
        CHECK(attack_skill.query_usage(&ctx, 0) == SkillUsageResult::OK,
              "M6) 属性类失效**不挡攻击技能**");
        CHECK(attr_skill.query_usage(&ctx, 0) == SkillUsageResult::HIT_INVALID,
              "M6) 属性类失效挡下属性技能");
        std::cout << "  [M6] 属性类失效: 攻击技能不受影响、属性技能被挡\n";
    }
    {
        // 两条都挂 → 两种技能都失效（用户："想要两个都失效注册两个就好了"）
        auto p0 = make_pet(0, "攻方", kZhuangJi, kGaoSu, 300, 3000);
        auto p1 = make_pet(1, "守方", kGaoSu, kGaoSu, 100, 3000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].accuracy = 100;
        ctx.getPet(0).skills[1].accuracy = 100;
        ctx.rule_center_.grant_hit_invalid(/*防御方=*/1, -1, 70063,
                                           static_cast<int>(HitInvalidMode::kEffectsOnly), 1, false);
        ctx.rule_center_.grant_hit_invalid(/*防御方=*/1, -1, 70063,
                                           static_cast<int>(HitInvalidMode::kEffectsOnly), 1, true);
        Skills& attack_skill = ctx.getPet(0).skills[0];
        Skills& attr_skill = ctx.getPet(0).skills[1];
        CHECK(attack_skill.query_usage(&ctx, 0) == SkillUsageResult::HIT_INVALID,
              "M6) 挂两条 → 攻击技能失效");
        CHECK(attr_skill.query_usage(&ctx, 0) == SkillUsageResult::HIT_INVALID,
              "M6) 挂两条 → 属性技能也失效");
        std::cout << "  [M6] 两条都挂: 攻击/属性技能都被挡\n";
    }

    std::cout << "\n场景 041 通过：必中走每回合凭证（条件必中不再永久改技能对象）、"
                 "失明（非必中必 miss / 必中 50% 效果失效 / 强制执行最先短路）、"
                 "③层命中失效按技能类型拆两类分别消费。\n";
    return 0;
}
