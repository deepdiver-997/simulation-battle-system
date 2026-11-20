// 场景 6380 —— J305 增伤+对手回血族（1883，moves_lib/j_amp_foe_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6380-6389）
//
// 断言口径：
//   - 1883（38003 渊影之欣快，纯链 [1883]|[30,4]）：本次攻击 +30% 通用增伤
//     （审计槽 991883100）；未击败 → 回合末对手回 maxHP/4（审计槽 991883200）。
//   - 回血观测：先把对手 hp 压到 10000（最大 20000）→ 回合末 +5000，再减本次红伤 1~2
//     → [14998,14999]。
//   - 击败档：hp=1 / atk=300 → 不注册回血（审计 0）。
//   - 封回血档：HEAL_BLOCK 票 → 回血原语挡下（对手 hp 只掉红伤）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <effects/damage_pipeline.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1883 = 38003;   // 渊影之欣快 [1883]|[30,4] 特150（纯链）
constexpr int kPaiDa = 10004;

constexpr int kAmpKey = 991883100;    // +owner
constexpr int kHealKey = 991883200;   // +owner
constexpr int kProbeAmpKey = 991883300;   // 场景探针：AMP 阶段读到的 ws 增伤值

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int atk) {
    ElfPet p = make_pet(1, "己方", {kCarrier1883, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
    p.hp = hp;
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// AMP 阶段探针：攻击方=0 的那一趟，记录 ws.damage_add_pct[0]（管线真实消费值）。
void register_amp_probe(BattleContext& ctx) {
    ctx.register_damage_effect(DamagePhase::DETECT, /*owner=*/1, DamageEffectCategory::DETECT,
                               [](BattleContext* c, int) {
                                   if (!c || c->resolvedDamage.attackerId != 0) {
                                       return;
                                   }
                                   c->plugin_storage[kProbeAmpKey] = static_cast<int>(c->ws.damage_add_pct[0]);
                               });
}

void run_round(ScenarioBattle& b) {
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

    // ══ T6380.a 未击败 → 回合末对手回 5000 ══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/1)), make_party(mk_foe(/*hp=*/10000)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_amp_probe(ctx);
        run_round(b);
        CHECK_EQ(stor(ctx, kAmpKey + 0), 30, "T6380.a 本刀通用增伤 +30%");
        CHECK_EQ(stor(ctx, kProbeAmpKey), 30,
                 "T6380.a AMP 阶段实际消费 ws.damage_add_pct = 30");
        CHECK_EQ(stor(ctx, kHealKey + 0), 5000, "T6380.a 回合末回 maxHP/4 = 5000");
        const int hp = ctx.getPet(1).hp;
        CHECK(hp >= 14996 && hp <= 14999,
              "T6380.a 对手 hp = 10000+5000−红伤1~3（+30%增伤，实测 " + std::to_string(hp) + "）");
    }

    // ══ T6380.b 击败 → 不注册回血 ══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/300)), make_party(mk_foe(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b);
        CHECK_EQ(ctx.getPet(1).hp, 0, "T6380.b 对手被击败");
        if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();   // 让回合结束真的跑完（回血 tick 若被注册就会执行）
        }
        CHECK_EQ(stor(ctx, kHealKey + 0), 0, "T6380.b 击败门 → 不回血");
    }

    // ══ T6380.c 对手封回血 → 回血被挡 ══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/1)), make_party(mk_foe(/*hp=*/10000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::HEAL_BLOCK, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/6380, /*soul_immunity=*/false);
        run_round(b);
        const int hp = ctx.getPet(1).hp;
        CHECK(hp >= 9996 && hp <= 9999,
              "T6380.c 封回血 → 仅红伤 1~3（实测 " + std::to_string(hp) + "）");
    }

    std::cout << "scenario 6380 OK" << std::endl;
    return 0;
}
