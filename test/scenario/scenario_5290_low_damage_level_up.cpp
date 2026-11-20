// 场景 5290 —— J204 低伤自身升级族（107，moves_lib/j_low_damage_level_up.cpp）
//（2026-10-22，号段 5290-5299）
//
// 断言口径：
//   - 107（双重暗影 12112 纯链 [107]|[250,0]，物 150；紫雾霜冻 11841 [100,2]，特 150）：
//     · (a) atk=1 → 本刀伤害 ≈3 < 250 → 自身攻槽 +1；审计 <250。
//     · (b) atk=300 → 本刀伤害 ≥250 → 不提升；审计 ≥250（绝对值断言）。
//     · (c) 槽位映射：11841 [100,2] 低伤 → 自身**防御槽** +1（非攻槽）。
//   - 断言读精确槽位 ability_levels[0][slot]，不用"等级和"（坑集 §1）。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40
constexpr int kShuangYing = 12112; // [107]|[250,0]
constexpr int kZiWu      = 11841; // [107]|[100,2]
constexpr int kAuditKey  = 990107100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

void use_carrier(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5290 107 低伤 → 自身指定槽 +1 ══
    {
        {   // (a) 低伤 → 攻槽 +1
            ScenarioBattle b(make_party(mk_owner(kShuangYing, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK(audit(ctx) >= 0 && audit(ctx) < 250,
                  "T5290.a 107：本刀伤害 <250（审计 " + std::to_string(audit(ctx)) + "）");
            CHECK_EQ(ctx.ability_levels[0][0], 1,
                     "T5290.a 107：自身攻槽 +1（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
        }
        {   // (b) 高伤 → 不提升
            ScenarioBattle b(make_party(mk_owner(kShuangYing, /*atk=*/300)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK(audit(ctx) >= 250,
                  "T5290.b 107：本刀伤害 ≥250（审计 " + std::to_string(audit(ctx)) + "）");
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T5290.b 107：高伤 → 不提升（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
        }
        {   // (c) 槽位映射：防御槽（2）
            ScenarioBattle b(make_party(mk_owner(kZiWu, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[0][2], 1,
                     "T5290.c 107：11841 [100,2] → 防御槽 +1（实测 " +
                         std::to_string(ctx.ability_levels[0][2]) + "）");
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T5290.c 107：攻槽不受影响（槽位未错位）");
        }
        std::cout << "  [T5290] 107 低伤升级 + 高伤对照 + 槽位映射 通过\n";
    }

    std::cout << "scenario_5290 PASS (J204 低伤自身升级族)" << std::endl;
    return 0;
}
