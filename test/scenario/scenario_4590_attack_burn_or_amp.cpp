#include <utils/sbs_rand.h>
// 场景 4590 —— K303 攻技烧伤+未烧伤反增伤族（616，soul_lib/k303_attack_burn_or_amp.cpp）
//（2026-10-21，worktree ../agents/k303；号段 4590-4599 → T4590-T4592 三块）
//
// 断言口径（对手 NONE）：
//   - T4590 正常档：对手干净 → 攻击后对手带烧伤、自身攻击等级 0（烧伤分支）。
//   - T4591 已烧伤档：对手预置烧伤 → 仍烧伤、攻击等级 0。
//   - T4592 无效码档：db[1]=99（非法码，实现守卫跳过施加）→ 对手无烧伤、
//     自身攻击 +2（增伤分支，args 驱动直证）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道）。
    SoulMark sm(616, "K303 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2977;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "玛格丽特", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, 800000, make_mark(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4590 正常档：必烧伤、不自增 ═══════════════════════════════════════════
    {
        sbs_srand(4590);
        ScenarioBattle b(make_party(mark_pet({1, 2, 0, 2})), make_party(foe_pet()));
        b.start();
        attack_round(b);
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T4590 616：攻击后对手带烧伤");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T4590 616：对手已烧伤 → 自攻不升（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        std::cout << "  [T4590] 必烧伤分支 通过\n";
    }

    // ══ T4591 已烧伤档：仍烧伤、不自增 ═════════════════════════════════════════
    {
        sbs_srand(4591);
        ScenarioBattle b(make_party(mark_pet({1, 2, 0, 2})), make_party(foe_pet()));
        b.start();
        apply_anomaly(&b.ctx(), 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        attack_round(b);
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T4591 616：对手保持烧伤");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T4591 616：对手已烧伤 → 自攻不升（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        std::cout << "  [T4591] 已烧伤分支 通过\n";
    }

    // ══ T4592 无效码档：施加被守卫跳过 → 自攻 +2 ═══════════════════════════════
    {
        sbs_srand(4592);
        ScenarioBattle b(make_party(mark_pet({1, 99, 0, 2})), make_party(foe_pet()));
        b.start();
        attack_round(b);
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, kBurn),
              "T4592 616：无效码 → 对手无烧伤");
        CHECK_EQ(ctx.ability_levels[0][0], 2,
                 "T4592 616：对手未烧伤 → 自攻 +2（实测 lv=" +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        std::cout << "  [T4592] 未烧伤自增分支 通过\n";
    }

    std::cout << "scenario_4590 PASS (K303 攻技烧伤+未烧伤反增伤族)" << std::endl;
    return 0;
}
