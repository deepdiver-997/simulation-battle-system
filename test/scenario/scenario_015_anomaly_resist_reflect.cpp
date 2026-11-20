// 场景 O：异常抗性接线（纠正优先级）+ 反弹/转化异常。
//
// 官方优先级（用户修正）：次免/回合类免疫(soul=false) → 异常抗性 → 魂免(soul=true)。
// 抗性成功会直接附加"免疫异常"异常(21, 2回合)——**击穿魂免**；该异常可被转化异常转换
// （绕过抗性+魂免直接转换的 exploit）。
//
// O1 抗性优先于魂免：抗性成功 → 挂"免疫异常"(21) 非烧伤（击穿魂免）；对照组抗性失败→魂免挡。
// O2 免疫异常可被转化（exploit）：21 → 焚烬(22)。
// O3 次免/回合类先于抗性：效果免疫先挡，不 roll 抗性、不挂 21。
// O4 反弹防循环：双方弹控+双方免疫 → 1 次反弹即止，不卡死。
// O5 转化正常路径：烧伤 → 麻痹。
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);                 // 烧伤 2
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);       // 麻痹 0
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);     // 焚烬 22
constexpr int kAbnormalImmunity = static_cast<int>(AbnormalStatusId::AbnormalImmunity);  // 免疫异常 21
constexpr int kSkillZhuangJi = 10001;  // 撞击

// 最小对战：双方都是同一普通精灵，直接对 ctx 调 apply_anomaly 原语。
scenario::ScenarioBattle make_battle() {
    auto pet = scenario::make_pet(1, "精灵", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                              kSkillZhuangJi, kSkillZhuangJi},
                                  /*speed=*/100, /*atk=*/100, /*hp=*/5000);
    return scenario::ScenarioBattle(scenario::make_party(pet), scenario::make_party(pet));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== O1 抗性优先于魂免 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        ctx.getPet(1).resistance.setUncontrolSlot(0, kBurn, 100);  // 烧伤抗性 100%
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/7001, /*soul_immunity=*/true);
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kBurn, 2, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::RESISTED_BY_RESISTANCE);
        REQUIRE(ctx.has_active_abnormal_status(1, kAbnormalImmunity));  // 击穿魂免挂了免疫异常
        REQUIRE(!ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [O1] 抗性优先于魂免: RESISTED, 挂免疫异常(21) 非烧伤 (击穿魂免)" << std::endl;

        // 对照组：抗性 0% → 抗性失败 → 魂免挡（不挂 21）
        auto battle2 = make_battle();
        BattleContext& ctx2 = battle2.ctx();
        ctx2.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, 7001, true);
        const ApplyAnomalyResult r2 = apply_anomaly(&ctx2, 1, kBurn, 2, 0);
        REQUIRE(r2 == ApplyAnomalyResult::TARGET_IMMUNE);
        REQUIRE(!ctx2.has_active_abnormal_status(1, kAbnormalImmunity));
        std::cout << "  [O1b] 抗性失败→魂免挡: TARGET_IMMUNE, 无免疫异常" << std::endl;
    }

    //========== O2 免疫异常可被转化（exploit）==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        ctx.getPet(1).resistance.setUncontrolSlot(0, kBurn, 100);
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, 7002, true);
        ctx.anomaly_conversion[1][kAbnormalImmunity] = kIncinerate;  // 攻击方预置：免疫异常→焚烬
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kBurn, 2, 0);
        REQUIRE(r == ApplyAnomalyResult::RESISTED_BY_RESISTANCE);
        REQUIRE(ctx.has_active_abnormal_status(1, kIncinerate));       // 已转成焚烬
        REQUIRE(!ctx.has_active_abnormal_status(1, kAbnormalImmunity));
        REQUIRE(!ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [O2] 免疫异常可被转化: RESISTED, 21→焚烬(22) (绕过抗性+魂免)" << std::endl;
    }

    //========== O3 次免/回合类先于抗性 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        ctx.getPet(1).resistance.setUncontrolSlot(0, kBurn, 100);
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                           /*duration_rounds=*/2, /*source_id=*/7003);  // effect 免疫（soul=false 默认）
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kBurn, 2, 0);
        REQUIRE(r == ApplyAnomalyResult::TARGET_IMMUNE);
        REQUIRE(!ctx.has_active_abnormal_status(1, kAbnormalImmunity));  // 不 roll 抗性
        REQUIRE(!ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [O3] 次免/回合类先于抗性: TARGET_IMMUNE, 不roll抗性" << std::endl;
    }

    //========== O4 反弹防循环（双方弹控）==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        ctx.grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0, 2, 7004);
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 2, 7005);
        // 双方弹控（并入 RuleCenter REFLECT：各自免疫时反弹给施放方）
        ctx.rule_center_.grant_reflect(/*source=*/0, /*slot=*/-1, /*eff=*/70040, /*target=*/0, 0);
        ctx.rule_center_.grant_reflect(/*source=*/1, /*slot=*/-1, /*eff=*/70041, /*target=*/1, 0);
        const ApplyAnomalyResult r = apply_anomaly(&ctx, /*target=*/1, kBurn, 2, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::REFLECTED);  // pet1 免疫 + 反弹回 pet0
        REQUIRE(!ctx.has_active_abnormal_status(0, kBurn));  // pet0 也免疫，不再弹
        REQUIRE(!ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [O4] 反弹防循环: REFLECTED, 1次反弹即止 (双方弹控不卡死)" << std::endl;
    }

    //========== O5 转化正常路径 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        ctx.anomaly_conversion[1][kBurn] = kParalysis;  // 烧伤→麻痹
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kBurn, 2, 0);
        REQUIRE(r == ApplyAnomalyResult::CONVERTED);
        REQUIRE(ctx.has_active_abnormal_status(1, kParalysis));
        REQUIRE(!ctx.has_active_abnormal_status(1, kBurn));
        std::cout << "  [O5] 转化正常路径: CONVERTED, 烧伤→麻痹" << std::endl;
    }

    std::cout << "scenario_o PASS (异常抗性优先级 + 免疫异常击穿魂免 + 反弹防循环 + 转化)"
              << std::endl;
    return 0;
}
