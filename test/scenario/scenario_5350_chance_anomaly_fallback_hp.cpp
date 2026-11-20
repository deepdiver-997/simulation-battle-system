// 场景 5350 —— J209 概率异常未触发→当前体力比例粉伤族（974，moves_lib/j_chance_anomaly_fallback_hp.cpp）
//（2026-10-22，号段 5350-5349）
//
// 断言口径：
//   - 974（睡意环绕 34302 纯链 [974]|[50,8,3]，特 130；己方 hp=5000）：
//     · 每局读审计 990974100 = ApplyAnomalyResult：
//       - ROLL_FAILED（未触发）→ 兜底粉伤 = 5000/3 = 1666，对手掉血 ≈1666+主伤；
//       - 已触发 → 对手睡眠(8) 且无兜底槽，对手仅掉主伤 [1,5]。
//     · 20 局（p=50）内两个分支都出现，且每局行为与审计自洽。
//   - 不猜随机结果：分支由审计决定，断言各分支的绝对量。
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
constexpr int kShuiYi  = 34302;  // [974]|[50,8,3]
constexpr int kResultKey = 990974100;
constexpr int kFallbackKey = 990974101;
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kShuiYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5350 974 概率异常 / 未触发兜底粉伤 ══
    {
        int fallback_trials = 0;
        int triggered_trials = 0;
        for (int trial = 0; trial < 20; ++trial) {
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            const int result = storage_int(ctx, kResultKey);
            if (result == static_cast<int>(ApplyAnomalyResult::ROLL_FAILED)) {
                ++fallback_trials;
                CHECK_EQ(storage_int(ctx, kFallbackKey), 1666,
                         "T5350 974：未触发兜底 = 当前体力 5000/3（trial " +
                             std::to_string(trial) + "）");
                CHECK(loss >= 1666 && loss <= 1672,
                      "T5350 974：未触发 → 对手掉血 ≈1666+主伤（实测 " +
                          std::to_string(loss) + "）");
            } else {
                ++triggered_trials;
                CHECK(ctx.has_active_abnormal_status(1, kSleep),
                      "T5350 974：触发 → 对手睡眠（trial " + std::to_string(trial) + "）");
                CHECK_EQ(storage_int(ctx, kFallbackKey), -1,
                         "T5350 974：触发 → 无兜底粉伤");
                CHECK(loss >= 1 && loss <= 5,
                      "T5350 974：触发 → 仅主伤 [1,5]（实测 " +
                          std::to_string(loss) + "）");
            }
        }
        CHECK(fallback_trials > 0, "T5350 974：20 局出现未触发兜底分支");
        CHECK(triggered_trials > 0, "T5350 974：20 局出现触发分支");
        std::cout << "  [T5350] 974 概率异常/兜底粉伤（fallback " << fallback_trials
                  << "/20，triggered " << triggered_trials << "/20）通过\n";
    }

    std::cout << "scenario_5350 PASS (J209 概率异常未触发→当前体力比例粉伤族)" << std::endl;
    return 0;
}
