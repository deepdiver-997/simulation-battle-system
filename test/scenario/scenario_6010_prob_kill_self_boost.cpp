#include <utils/sbs_rand.h>
// 场景 6010 —— J270 秒杀兜底自强化族（1591，moves_lib/j_prob_kill_self_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6010-6019 → T6010.a-c 三块）
//
// 断言口径：
//   - 纯链载体 混沌齿轮 35530 [1591]|[10,1]（特130，atk=1000/def=100）。
//   - 两半走**运行期种子自校准**（J145 1318 场景同款：掷点 rand 序位随种子漂移）：
//     中 → 对手 hp==0（force_hp_to_zero）；歪 → 自身六槽各 +1（逐槽快照）。
//   - 审计槽 991591000+owner = 1（秒杀）/ 0（兜底），用于锁"真的走了对应分支"。
//   - 起始 +6 满 cap 档：兜底照常触发但槽位钳住不变（坑集 §1 口径：起始即满 cap
//     时"生效后不变"算通过）。
#include <any>
#include <cstdlib>
#include <iostream>
#include <string>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHunDun = 35530;   // 混沌齿轮：特130 纯链 [1591]|[10,1]
constexpr int kPaiDa  = 10004;   // 拍打：filler
constexpr int kMaxHp  = 99999;

constexpr int kAuditBase = 991591000;   // +owner：1=秒杀 / 0=兜底

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kHunDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

void run_once(ScenarioBattle& b, int seed) {
    sbs_srand(seed);
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

    int seed_kill = -1;
    int seed_fallback = -1;

    // ══ T6010.a 分支自校准：循环种子直到秒杀/兜底两半都实测到 ══
    for (int s = 1; s < 800 && (seed_kill < 0 || seed_fallback < 0); ++s) {
        if (seed_kill < 0) {
            ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            run_once(b, s);
            bool levels_zero = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                levels_zero = levels_zero && ctx.ability_levels[0][i] == 0;
            }
            if (ctx.getPet(1).hp <= 0 && audit_value(ctx, kAuditBase + 0) == 1
                && levels_zero) {
                seed_kill = s;
            }
        }
        if (seed_fallback < 0) {
            ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            run_once(b, s);
            bool all_plus_one = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                all_plus_one = all_plus_one && ctx.ability_levels[0][i] == 1;
            }
            if (ctx.getPet(1).hp > 0 && audit_value(ctx, kAuditBase + 0) == 0
                && all_plus_one) {
                seed_fallback = s;
            }
        }
    }
    CHECK(seed_kill > 0, "T6010.a 1591：掷中 → 对手 hp==0（自校准种子找到）");
    CHECK(seed_fallback > 0, "T6010.a 1591：掷歪 → 自身六槽 +1（自校准种子找到）");

    // ══ T6010.b 兜底分支逐槽快照：六槽各 +1、对手侧不动、审计 0 ══
    if (seed_fallback > 0) {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int before_self[BattleContext::kAbilityLevelSlotCount] = {};
        int before_foe[BattleContext::kAbilityLevelSlotCount] = {};
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            before_self[i] = ctx.ability_levels[0][i];
            before_foe[i] = ctx.ability_levels[1][i];
        }
        run_once(b, seed_fallback);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], before_self[i] + 1,
                     "T6010.b 1591：兜底自身槽+" + std::to_string(i));
            CHECK_EQ(ctx.ability_levels[1][i], before_foe[i],
                     "T6010.b 1591：对手槽不变 " + std::to_string(i));
        }
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 0, "T6010.b 1591：审计=兜底");
        CHECK(ctx.getPet(1).hp > 0, "T6010.b 1591：对手存活");
        std::cout << "  [T6010.b] 1591 兜底六槽 通过 (seed=" << seed_fallback << ")\n";
    }

    // ══ T6010.c 起始即 +6 满 cap：兜底照常触发，槽位钳住不变（合法通过态）══
    if (seed_fallback > 0) {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[0][i] = 6;
        }
        run_once(b, seed_fallback);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 6,
                     "T6010.c 1591：满 cap 槽不变 " + std::to_string(i));
        }
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 0, "T6010.c 1591：审计=兜底（仍触发）");
        CHECK(ctx.getPet(1).hp > 0, "T6010.c 1591：对手存活");
        std::cout << "  [T6010.c] 1591 满 cap 钳位 通过\n";
    }

    std::cout << "scenario_6010 PASS (J270 秒杀兜底自强化族)"
              << " seeds=" << seed_kill << "/" << seed_fallback << std::endl;
    return 0;
}
