// （G2 派发注意：721 已被本场景占用，G2 场景用 720/722-729）
// 场景 721 —— effect 1850 因子解离 37092 攻击增伤回归（2026-09-29，G1 发现的
// off-by-one 修复验证：消费点 query_usage 先于应用点 ATTACK_DAMAGE，原实现授
// count=1 在查询时被扣、应用读到 0 → 增伤完全不落地；修复 = 授 count+1 补偿，
// 1179 同款。见 batch_c_skills.cpp effect_1850_grant_flush 注释）。
#include <iostream>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>
#include "scenario_harness.h"
using namespace scenario;
namespace {
constexpr int kYinzi = 37092;    // 因子解离：物攻 140 [1211,1850] 1850切片=[1,50]
constexpr int kZhuangJi = 10001;
ElfPet atk_pet(int speed = 300, int atk = 600) {
    ElfPet p = make_pet(1, "攻方", {kYinzi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, 5000);
    for (Skills& s : p.skills) { s.must_hit = 1; s.accuracy = 100; }
    return p;
}
ElfPet def_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, 1, 1000000);
    for (Skills& s : p.skills) { s.must_hit = 1; s.accuracy = 100; }
    return p;
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    // 主场：R1 对手挂可断票 → 因子解离消成功（授 count=1）→ R2 攻方普攻 → 记伤害
    ScenarioBattle b(make_party(atk_pet()), make_party(def_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    seal_skill(&ctx, 1, 0, 70721, true, false, 0, 3, true, -1, EffectScope::ON_STAGE, false, 100, false);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 普攻（应 +50%）
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int boosted = foe_before - ctx.getPet(1).hp;
    // 基线场：对手无票 → 消除失败 → 不授予 → R2 同口径普攻
    ScenarioBattle c(make_party(atk_pet()), make_party(def_pet()));
    c.start();
    c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    c.drive();
    const int base_before = c.ctx().getPet(1).hp;
    c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    c.drive();
    const int base = base_before - c.ctx().getPet(1).hp;
    // 断言：+50% 增伤生效（随机浮动 85%~100% → 比例 ∈ [1.27, 1.5]；未修复时 ≈1.0）
    CHECK(boosted > base * 115 / 100,
          "721 核心：下1次攻击 +50% 增伤落地（修复 off-by-one 后比例 >1.15）");
    std::cout << "boosted=" << boosted << " base=" << base
              << " ratio=" << (base ? double(boosted) / base : 0) << "\n";
    std::cout << "scenario_721 PASS (1850 攻击增伤 off-by-one 修复验证)" << std::endl;
    return 0;
}
