// 场景 6360 —— J303 受击阈值异常/固伤兜底窗族（1612，moves_lib/j_takehit_threshold_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6360-6369）
//
// 断言口径：
//   - 1612（27296 深渊凝视 [911,1612,1304,843]，1612=[3,300,100,23,300]）：
//     窗 [R+1,R+3]，自己受到 <300 伤害时 100% 令对手诅咒(23)；未触发则 300 固伤。
//   - 阈值档用对手 atk=2000（红伤 ≈[580,680] ≥300）→ 不触发。
//   - 未触发档用概率闸门把申报概率压到 0（ROLL_FAILED）→ 兜底 300 固伤。
//   - 窗界：R2/R3/R4 三跳、R5 不再触发（N=3）。
//   - 审计槽 991612100（触发次数）/991612200（结果码）/991612300（兜底固伤）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1612 = 27296;   // 深渊凝视 [911,1612,1304,843]|[1,3,300,100,23,300,2,20,2,2]
constexpr int kPaiDa = 10004;
constexpr int kCurse = 23;

constexpr int kTriggersKey = 991612100;   // +owner
constexpr int kResultKey = 991612200;     // +owner
constexpr int kFallbackKey = 991612300;   // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier1612, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/20000);
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

void force_roll_failed(BattleContext& ctx, int protected_side) {
    ctx.grant_chance_gate(protected_side, 6360 + protected_side, ChanceTag::AnomalyAttach,
                          /*threshold_pct=*/101, /*below_result=*/0, /*above_result=*/-1,
                          /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                          /*rounds=*/0);
}

void cast_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6360.a 低于阈值 + 100% → 对手诅咒、无兜底 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        cast_round(b);        // R1 开窗
        foe_attack_round(b);  // R2 受击约 29~34 < 300
        CHECK_EQ(stor(ctx, kTriggersKey + 0), 1, "T6360.a 触发 1 次");
        CHECK_EQ(stor(ctx, kResultKey + 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6360.a 100% 异常落地");
        CHECK(ctx.has_active_abnormal_status(1, kCurse), "T6360.a 对手诅咒(23)");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6360.a 已触发 → 无兜底");
        CHECK_EQ(ctx.getPet(1).hp, 20000, "T6360.a 对手未被固伤");
    }

    // ══ T6360.b 伤害 ≥ 阈值 → 不触发 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        cast_round(b);
        foe_attack_round(b);   // 红伤约 580~680 ≥ 300
        CHECK_EQ(stor(ctx, kTriggersKey + 0), 0, "T6360.b 阈值门闭 → 0 触发");
        CHECK(!ctx.has_active_abnormal_status(1, kCurse), "T6360.b 对手无诅咒");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6360.b 无兜底");
    }

    // ══ T6360.c 未触发（闸门压 0）→ 兜底 300 固伤 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_roll_failed(ctx, 1);
        cast_round(b);
        const int hp_before = ctx.getPet(1).hp;
        foe_attack_round(b);
        CHECK_EQ(stor(ctx, kTriggersKey + 0), 1, "T6360.c 触发 1 次");
        CHECK_EQ(stor(ctx, kResultKey + 0),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T6360.c 概率未过门");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 300, "T6360.c 兜底 300 固伤");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(loss, 300, "T6360.c 对手掉血 = 300（实测 " + std::to_string(loss) + "）");
    }

    // ══ T6360.d 窗界：R2/R3/R4 触发，R5 不触发 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_roll_failed(ctx, 1);
        cast_round(b);   // R1 开窗
        const int hp_before = ctx.getPet(1).hp;
        for (int r = 2; r <= 4; ++r) {
            foe_attack_round(b);
        }
        CHECK_EQ(stor(ctx, kTriggersKey + 0), 3, "T6360.d R2-R4 共 3 次触发");
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 900, "T6360.d 3×300 = 900 固伤");
        foe_attack_round(b);   // R5 窗外
        CHECK_EQ(stor(ctx, kTriggersKey + 0), 3, "T6360.d R5 窗外不触发");
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 900, "T6360.d R5 无额外固伤");
    }

    std::cout << "scenario 6360 OK" << std::endl;
    return 0;
}
