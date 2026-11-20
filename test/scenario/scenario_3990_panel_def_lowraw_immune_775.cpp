// 场景 3990 —— K249 面板双防+低伤门免异常族（775，soul_lib/k249_panel_def_lowraw_immune_775.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 3990-3999 → T3990 一块三例）
//
// 断言口径：
//   - a 面板双防：max 99999 → 双防各 +19999 → numericalBase[DEF/SPDEF] = 20099 精确。
//   - b 低伤门正例：raw ~90 < 300 → R1 末授予免疫 → R2 直铺烧伤被拒（TARGET_IMMUNE）。
//   - c 低伤门负例：atk 1500 → raw > 300 → 无免疫 → R2 直铺烧伤落地。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark() {
    SoulMark sm(775, "K249 魂印", "test", EffectArgs(std::vector<int>{20, 300}));
    sm.monster_id = 3116;   // 班纳尔克
    return sm;
}

ElfPet mark_pet(int atk) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, atk, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
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

    // ══ T3990.a 面板双防 +19999（max×20%）══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect = 100 + 99999 * 20 / 100;
        CHECK_EQ(ctx.getPet(0).numericalBase[NumericalPropertyIndex::DEFENSE],
                 expect, "T3990.a 防御面板 +max×20%");
        CHECK_EQ(ctx.getPet(0).numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE],
                 expect, "T3990.a 特防面板 +max×20%");
        std::cout << "  [T3990.a] 面板双防 通过（" << expect << "）\n";
    }

    // ══ T3990.b 低伤门正例：raw < 300 → 下回合免异常 ══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);   // raw ~90 < 300 → R1 末授予免疫
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 3, 1),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T3990.b R2 烧伤被免疫拒绝");
        std::cout << "  [T3990.b] 低伤门免异常 通过\n";
    }

    // ══ T3990.c 低伤门负例：raw > 300 → 无免疫 ══
    {
        ScenarioBattle b(make_party(mark_pet(1500)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);   // raw ~315+ > 300 → 不授予
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 3, 1),
                 ApplyAnomalyResult::SUCCESS, "T3990.c 高伤 → R2 烧伤正常落地");
        std::cout << "  [T3990.c] 高伤负例 通过\n";
    }

    std::cout << "scenario_3990 PASS (K249 面板双防+低伤门免异常族)" << std::endl;
    return 0;
}
