// 场景 5360 —— J210 体力优势门概率异常→未触发比例粉伤族（1570，moves_lib/j_high_hp_gate_anomaly_fallback.cpp）
//（2026-10-22，号段 5360-5359）
//
// 断言口径：
//   - 1570（重装践踏 38288 链 [1570,485]，1570 段 [70,7,4]，物 85）：
//     · (a) 自身 hp=5000 > 对手 3000：20 局按审计 991570000（ApplyAnomalyResult）分支：
//       未触发 → 兜底粉伤 3000/4=750（对手掉血 ≈750+主伤）；触发 → 对手疲惫(7)、
//       仅主伤 [1,4]。两个分支都要出现。
//     · (b) 自身 hp=1000 < 对手 5000 → 门不成立：审计缺席、无异常、仅主伤。
//     · (c) 双方 hp 相等（5000==5000）→ 严格"高于"不成立：审计缺席。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;
constexpr int kZhongZhuang = 38288;  // [1570,485]|[70,7,4]
constexpr int kResultKey = 991570000;
constexpr int kFallbackKey = 991570001;
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int hp) {
    ElfPet p = make_pet(1, "己方", {kZhongZhuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int storage_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

int run_once(int owner_hp, int foe_hp, bool& gate_result, int& result_code) {
    ScenarioBattle b(make_party(mk_owner(100000)), make_party(mk_foe(foe_hp)));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = owner_hp;   // 只压当前体力
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    gate_result = (storage_int(ctx, kResultKey) >= 0);
    result_code = storage_int(ctx, kResultKey);
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5360 1570 体力优势门 / 概率异常 / 未触发比例粉伤 ══
    {
        int fallback_trials = 0;
        int triggered_trials = 0;
        for (int trial = 0; trial < 20; ++trial) {
            ScenarioBattle b(make_party(mk_owner(/*hp=*/5000)), make_party(mk_foe(/*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            const int result = storage_int(ctx, kResultKey);
            CHECK(result >= 0, "T5360.a 1570：门成立 → 审计存在（trial " +
                                   std::to_string(trial) + "）");
            if (result == static_cast<int>(ApplyAnomalyResult::ROLL_FAILED)) {
                ++fallback_trials;
                CHECK_EQ(storage_int(ctx, kFallbackKey), 750,
                         "T5360.a 1570：未触发兜底 = max_hp(对手)/4 = 750");
                CHECK(loss >= 750 && loss <= 756,
                      "T5360.a 1570：未触发 → 对手掉血 ≈750+主伤（实测 " +
                          std::to_string(loss) + "）");
            } else {
                ++triggered_trials;
                CHECK(ctx.has_active_abnormal_status(1, kFatigue),
                      "T5360.a 1570：触发 → 对手疲惫(7)");
                CHECK_EQ(storage_int(ctx, kFallbackKey), -1,
                         "T5360.a 1570：触发 → 无兜底粉伤");
                CHECK(loss >= 1 && loss <= 4,
                      "T5360.a 1570：触发 → 仅主伤 [1,4]（实测 " +
                          std::to_string(loss) + "）");
            }
        }
        CHECK(fallback_trials > 0, "T5360.a 1570：20 局出现未触发兜底分支");
        CHECK(triggered_trials > 0, "T5360.a 1570：20 局出现触发分支");

        {   // (b) 低体：门不成立
            bool gate = true;
            int code = -2;
            const int loss = run_once(/*owner_hp=*/1000, /*foe_hp=*/5000, gate, code);
            CHECK(!gate, "T5360.b 1570：自身 1000 < 对手 5000 → 门不成立（审计缺席）");
            CHECK(loss >= 1 && loss <= 4,
                  "T5360.b 1570：仅主伤 [1,4]（实测 " + std::to_string(loss) + "）");
        }
        {   // (c) 相等：严格高于不成立
            bool gate = true;
            int code = -2;
            const int loss = run_once(/*owner_hp=*/5000, /*foe_hp=*/5000, gate, code);
            CHECK(!gate, "T5360.c 1570：双方 hp 相等 → 严格门不成立");
            CHECK(loss >= 1 && loss <= 4,
                  "T5360.c 1570：相等时仅主伤（实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T5360] 1570 体力门/概率异常/兜底粉伤（fallback " << fallback_trials
                  << "/20，triggered " << triggered_trials << "/20）+ 低体/相等对照 通过\n";
    }

    std::cout << "scenario_5360 PASS (J210 体力优势门概率异常→未触发比例粉伤族)" << std::endl;
    return 0;
}
