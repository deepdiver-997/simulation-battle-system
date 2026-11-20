// 场景 2070 —— J113 申报制未触发必暴/随机固伤吸取族（2 宏，moves_lib/j_anom_crit_rand.cpp）
//（2026-10-03，worktree ../agents/j113；号段 2070-2079 → T2070-T2071 两块）
//
// 断言口径（免费额度收尾档：只做确定性断言）：
//   - 1033（崩坏世纪 [50,19,2] 物150 纯链）：对手挂 ANOMALY 免疫票 → 异常必不落地
//     → must_crit NextRounds(2) → R2 攻击（crit_rate=0）crit_happened 置位；
//   - 1144（利爪重威破 [150,300] 物150 纯链）：fixed 无浮动 → 自身回 dealt∈[150,300]
//     （预置 4000 血 → [4150,4300]）；对手掉血 [148,304]（含红伤~2）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kBengHuai = 31605; // 崩坏世纪 [1033]|[50,19,2]：物150（申报制载体）
constexpr int kLiZhua  = 31414;  // 利爪重威破 [1144]|[150,300]：物150（随机固伤吸取载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2070 1033 未触发 → 下 2 回合必暴（崩坏世纪 [50,19,2]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kBengHuai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // ANOMALY 免疫票 → 异常必不落地 → 走"未触发"分支（确定性）
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/3,
                           /*source_id=*/113, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0,
                           /*source_effect_id=*/113);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, 19),
              "T2070.a 前置：异常 19 被免疫（未触发）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.crit_happened[0],
              "T2070.a 1033：未触发 → 下 2 回合必暴（crit_rate=0 下旗置位）");
        std::cout << "  [T2070] 1033 未触发必暴 通过\n";
    }

    // ══ T2071 1144 随机固伤+等量吸取（利爪重威破 [150,300]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kLiZhua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4000;                  // 预置缺口（防回吸钳制）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int gain = ctx.getPet(0).hp - 4000;
        const int foe_loss = 99999 - ctx.getPet(1).hp;
        CHECK(gain >= 148 && gain <= 300,
              "T2071.a 1144：吸取=dealt（实测回 " + std::to_string(gain) + "）");
        CHECK(foe_loss >= 148 && foe_loss <= 304,
              "T2071.a 1144：随机固伤 [150,300]+红伤（实测掉 " +
                  std::to_string(foe_loss) + "）");
        std::cout << "  [T2071] 1144 随机固伤吸取 通过\n";
    }

    std::cout << "scenario_2070 PASS (J113 申报制未触发必暴/随机固伤吸取族 1033/1144)" << std::endl;
    return 0;
}
