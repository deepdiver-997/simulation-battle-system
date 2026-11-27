#include <utils/sbs_rand.h>
// 场景 5980 —— J267 异常计数增伤族（2073，moves_lib/j_anomaly_count_amp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5980-5989 → T5980.a-d 四块）
//
// 断言口径：
//   - 纯链载体 凝血寒浪 37703 [2073]|[50]（特140，atk=1000/def=100）。
//   - 同种子对拍（J51 1103 场景口径）：sbs_srand(20265980) 后 drive，
//     n 种异常 → 伤害 = 基线 × (100 + n×50)/100（±3 取整余量）。
//   - 异常用 apply_anomaly 直施加（害怕 6 / 疲惫 7，均为控制类无回合末扣血）。
//   - 审计槽 991207300+owner = n×50（0=对手无异常）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kNingXue = 37703;   // 凝血寒浪：特 [2073]|[50]（纯链）
constexpr int kPaiDa   = 10004;   // 拍打：filler
constexpr int kMaxHp   = 99999;
constexpr int kSeed    = 20265980;

constexpr int kAuditBase = 991207300;   // +owner：n×50

constexpr int kFear    = static_cast<int>(AbnormalStatusId::Fear);       // 6
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);    // 7
constexpr int kPoison  = static_cast<int>(AbnormalStatusId::Poison);     // 1（非控制）
constexpr int kBleed   = static_cast<int>(AbnormalStatusId::Bleed);      // 16（非控制）

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
    ElfPet p = make_pet(1, "己方", {kNingXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

int run_attack(ScenarioBattle& b) {
    b.ctx().seed_rng(kSeed);   // 固定伤害浮动序列（同种子对拍）
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return b.ctx().resolvedDamage.final;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    int red_plain = 0;
    // ══ T5980.a 基线：对手无异常 → bonus 0，不写增伤槽 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        red_plain = run_attack(b);
        CHECK(red_plain > 0, "T5980.a 前置：基线红伤落地");
        CHECK_EQ(audit_value(b.ctx(), kAuditBase + 0), 0,
                 "T5980.a 2073：无异常 → bonus 0");
        std::cout << "  [T5980.a] 2073 基线 通过 (red=" << red_plain << ")\n";
    }

    // ══ T5980.b 对手 1 种异常 → +50% ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, kFear, 3, 0) == ApplyAnomalyResult::SUCCESS);
        const int red = run_attack(b);
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 50,
                 "T5980.b 2073：1 种异常 → bonus 50");
        CHECK(red * 2 >= red_plain * 3 - 3 && red * 2 <= red_plain * 3 + 3,
              "T5980.b 2073：同种子对拍恰 ×1.5（red=" + std::to_string(red) +
                  " plain=" + std::to_string(red_plain) + "）");
        std::cout << "  [T5980.b] 2073 1 异常 +50% 通过\n";
    }

    // ══ T5980.c 对手 2 种异常 → +100% ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, kFear, 3, 0) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(apply_anomaly(&ctx, 1, kFatigue, 3, 0) == ApplyAnomalyResult::SUCCESS);
        const int red = run_attack(b);
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 100,
                 "T5980.c 2073：2 种异常 → bonus 100");
        CHECK(red >= red_plain * 2 - 3 && red <= red_plain * 2 + 3,
              "T5980.c 2073：同种子对拍恰 ×2（red=" + std::to_string(red) +
                  " plain=" + std::to_string(red_plain) + "）");
        std::cout << "  [T5980.c] 2073 2 异常 +100% 通过\n";
    }

    // ══ T5980.d 门计数对手而非自身：自身 2 异常、对手干净 → 基线 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 用非控制类异常（中毒/流血），自身照常出招，只验计数侧位。
        REQUIRE(apply_anomaly(&ctx, 0, kPoison, 3, 1) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(apply_anomaly(&ctx, 0, kBleed, 3, 1) == ApplyAnomalyResult::SUCCESS);
        const int red = run_attack(b);
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 0, "T5980.d 2073：自身异常不计数");
        CHECK_EQ(red, red_plain, "T5980.d 2073：同种子伤害=基线");
        std::cout << "  [T5980.d] 2073 计数侧位 通过\n";
    }

    std::cout << "scenario_5980 PASS (J267 异常计数增伤族)" << std::endl;
    return 0;
}
