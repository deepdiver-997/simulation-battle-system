#include <utils/sbs_rand.h>
// 场景 3150 —— K182 effect 279（异常时限族）：缪斯/赤瞳缪斯
// "所有自身受到的异常状态只持续一回合"（零参）
//
// 可观测面：g_core->apply_anomaly_checked 直接给持有方种长时长异常（烧伤2、3 回合、
// 100%），对照有无魂印的存活回合数。
//   A 对照：无魂印 → 烧伤撑过第 1 回合（duration 3）。
//   B 持印（缪斯 1633）→ cap=1 → 第 1 回合末熄灭。
//   C 第二行同构（赤瞳缪斯 2527）。
//   D 持印后再种 → 每次都只活一回合（cap 常驻幂等写）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害、无异常干扰）
constexpr int kBurn = 2;         // AbnormalStatusId::Burn

SoulMark make_mark(int monster_id) {
    SoulMark sm(279, "K279", "test", EffectArgs());
    sm.monster_id = monster_id;
    return sm;
}

// 0 方（可带魂印）先空跑 warmup 回合（ROUND_START 节点首回合在 drive 才跑——
// start() 不触发，实测 cap0=0），再种 3 回合烧伤（cap 钳到 1），drive rounds 回合，
// 返回末尾是否仍烧伤。
bool probe_burn_after_rounds(int monster_id, bool with_mark, int rounds) {
    ElfPet holder = scenario::make_pet(1, "缪斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                       with_mark ? make_mark(monster_id) : SoulMark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();   // 首驱：ROUND_START 节点跑过，cap 已写
    (void)apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1, /*chance_pct=*/100,
                        ChanceSource::Skill);
    for (int r = 1; r <= rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return ctx.has_active_abnormal_status(0, kBurn);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 对照：无魂印 → 烧伤撑过种后 1 驱 ════════════════════════════════
    CHECK_EQ(probe_burn_after_rounds(1633, /*with_mark=*/false, 1), true,
             "A 对照：无魂印烧伤存活（duration 3）");

    // ══ B 持印（1633）→ 第 1 回合末熄灭 ═══════════════════════════════════
    CHECK_EQ(probe_burn_after_rounds(1633, true, 1), false, "B cap=1：1 回合末熄灭");

    // ══ C 第二行同构（2527）═══════════════════════════════════════════════
    CHECK_EQ(probe_burn_after_rounds(2527, true, 1), false, "C 赤瞳缪斯同行为");

    // ══ D 第 2 回合再种 → 仍只活一回合 ════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "缪斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(1633));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        // r1 空跑（建立 cap 常驻）
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // r2 开局种 3 回合烧伤 → r2 末应熄灭
        (void)apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1, /*chance_pct=*/100,
                            ChanceSource::Skill);
        CHECK_EQ(ctx.has_active_abnormal_status(0, kBurn), true, "D r2 种烧伤成功");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.has_active_abnormal_status(0, kBurn), false, "D r2 末熄灭（cap 常驻）");
    }

    std::cout << "scenario_3150 PASS (K182 279 异常时限一回合)" << std::endl;
    return 0;
}
