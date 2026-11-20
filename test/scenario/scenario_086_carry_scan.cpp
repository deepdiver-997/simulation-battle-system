#include <utils/sbs_rand.h>
// 场景 086 —— 携带类效果装载与开场扫描（"携带此技能时魂印/专属特性…"族）
//
// 锁的是 2026-09-19 的三段机制（台账"携带类技能强化"行）：
//   A. 注册表：CARRY_EFFECT 等价的 registerCarryEffect 按 effect_id 装载；未注册的
//      effect id 扫描不触发
//   B. 扫描：战斗开始（ENTER_EXIT_STAGE 桶之前）遍历全部精灵全部技能的
//      rawEffectRecords，命中注册表即执行 fn(ctx, owner, skill_slot, effect_id)
//   C. 载荷：典型动作为授改写票——本场景的测试 fn 授一张 ANOMALY_POOL_MOD 票，
//      验证"携带即改变随机异常附加池"的端到端链路
//
// 官方依据：skill 37198 圣元遗光阵（圣灵守护奥斯卡 3425 专属）side_effect 含
// effect 2125「携带此技能时，专属特性中对手属性技能无效的概率提升至100%」——
// 固有为族的真实样例；effect_icon 968/语料 idx=478（改写票的真实载荷形状）。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;          // 撞击（无 side_effect，对照组）
constexpr int k37198 = 37198;          // 圣元遗光阵：side_effect 含 2125
constexpr int kCarry2125 = 2125;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }
std::array<int, 5> carry_skills() { return {k37198, kSkill, kSkill, kSkill, kSkill}; }

ElfPet plain_pet(int hp = 2000) {
    return make_pet(1, "普通", plain_skills(), 100, 1, hp);
}

ElfPet carrier_pet(int hp = 2000) {
    return make_pet(3425, "奥斯卡样例", carry_skills(), 100, 1, hp);
}

// 测试载荷：把"携带者所在方"的随机异常附加池定向改写为诅咒（真实改写票的形状）。
void test_carry_2125(BattleContext* ctx, int owner, int skill_slot, int effect_id) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    ctx->rule_center_.grant_anomaly_pool_mod(
        owner, effect_id,
        [owner](BattleContext*, const std::vector<int>&) {
            (void)owner;
            return std::vector<int>{kCurse};
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A+B. 注册表命中 → 开场扫描执行（携带者 o=0 槽 0）
    // ════════════════════════════════════════════════════════════════
    {
        EffectFactory::getInstance().registerCarryEffect(kCarry2125, &test_carry_2125);
        std::array<ElfPet, 6> own = {carrier_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        // 载荷票已授：池被定向改写为诅咒
        const std::vector<int> resolved =
            x.rule_center_.resolve_anomaly_pool(&x, anomaly_pool_all());
        CHECK_EQ(resolved.size(), 1, "B1 携带票已授：全池被改写为单元素");
        CHECK_EQ(resolved[0], kCurse, "B1 改写目标 = 诅咒");

        // 对照：未注册 carry 的技能（撞击）不触发任何东西——
        // 用一个全新 ctx 验证"没带 37198 → 无改写票"
        std::array<ElfPet, 6> plain_own = {plain_pet(), plain_pet(), plain_pet(),
                                           plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle c(plain_own, scenario::make_party(plain_pet()));
        c.start();
        BattleContext& y = c.ctx();
        const std::vector<int> unresolved =
            y.rule_center_.resolve_anomaly_pool(&y, anomaly_pool_all());
        CHECK(unresolved.size() > 20, "A2 未携带者：池保持原样（扫描不误触发）");

        std::cout << "  [A+B] 携带扫描与改写票载荷通过\n";
    }

    std::cout << "scenario_086 全部通过\n";
    return 0;
}
