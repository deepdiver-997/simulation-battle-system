// 场景 2390 —— J141 解除异常增伤族（1 宏，moves_lib/j_cure_next_amp.cpp）
//（2026-10-04，worktree ../agents/j141；号段 2390-2399 → T2390 两例）
//
// 断言口径（双局对照，比值 = 倍率 × [0.85,1/0.85] 随机带——J23 1080 同式）：
//   - a 落地：自方预挂中毒（apply_anomaly 直调）→ R1 载体（16328 [1,100]）施放
//     治愈成功 → R2 同载体攻击 +100%（比值带 [170,235]%）；中毒 tick 只在 R1
//     行动起点结算一次（治愈后 R2 无 tick，不碍比值）；
//   - b 未中毒：治愈失败 → R2 无增伤（比值 < 150%，增伤局必然 ≥170% 可判别）。
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

constexpr int kXuanTian = 16328;  // 玄天凤坠闪 [631]|[1,100]：物? 纯链（治愈中毒→下回合+100%）
constexpr int kPaiDa    = 10004;  // 拍打：填充位
constexpr int kPoison   = static_cast<int>(AbnormalStatusId::Poison);  // 1
constexpr int kMaxHp    = 800000;  // 毒 tick 1/8=100000 整（自承，无断言）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kXuanTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    accuracy_100(p, true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p, false);
    return p;
}

// 两回合局：R1 自方载体（治愈判定）+ 对手 NONE；R2 自方再打载体 + 对手 NONE。
// 返回 [R1 对手掉血, R2 对手掉血]。
std::pair<int, int> probe(bool poison_self_first) {
    ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (poison_self_first) {
        (void)apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/9, /*actor=*/1);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    const int hp1 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return {kMaxHp - hp1, hp1 - ctx.getPet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2390.a 中毒治愈 → 下回合增伤 ×2（双局同载体对照）════════════════════
    {
        const auto [r1, r2] = probe(/*poison_self_first=*/true);
        CHECK(r1 > 50, "T2390.a 前置：R1 正伤（实测 " + std::to_string(r1) + "）");
        const int ratio_x100 = r2 * 100 / r1;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T2390.a 631：治愈成功下回合 +100%（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；R1=" + std::to_string(r1) + " R2=" + std::to_string(r2) + "）");
    }
    // ══ T2390.b 未中毒 → 治愈失败无增伤 ══════════════════════════════════════
    {
        const auto [r1, r2] = probe(/*poison_self_first=*/false);
        CHECK(r1 > 50, "T2390.b 前置：R1 正伤（实测 " + std::to_string(r1) + "）");
        const int ratio_x100 = r2 * 100 / r1;
        CHECK(ratio_x100 < 150,
              "T2390.b 631：无状态可治不增伤（比值 " + std::to_string(ratio_x100) +
                  "% < 150；若误增伤必然 ≥170）");
    }

    std::cout << "scenario_2390 PASS (J141 解除异常增伤族 631)" << std::endl;
    return 0;
}
