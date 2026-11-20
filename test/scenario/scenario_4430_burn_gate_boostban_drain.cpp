#include <utils/sbs_rand.h>
// 场景 4430 —— K287 烧伤门禁提升+周期吸取族（657，soul_lib/k287_burn_gate_boostban_drain.cpp）
//（2026-10-21，worktree ../agents/k287；号段 4430-4439 → T4430-T4432 三块）
//
// 断言口径（max=800000 → 吸取 200000；对手 NONE 无扰动）：
//   ⚠️ 对手带烧伤时有烧伤回合扣减点伤害叠加 → 对手掉血只断下界（≥吸取量），
//   自身回血量精确（烧伤在对手身上，不碰印主）。
//   - T4430 门开：对手烧伤 → 自回血恰 200000；行为门：对手回合内用精准（自身
//     命中+1）被禁提升票整次拦下（等级不动）。
//   - T4431 门闭：对手无异常 → 无吸取（对手血不变）、对手精准正常 +1。
//   - T4432 门翻：烧伤 1 回合窗 → R1 吸取、R2（烧伤到期）不再吸取（对手血 R2 不变）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kZhunQue = 20138;   // 精准：自身命中+1（禁提升票的行为探针）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [2,4]：db[0]=2 门码（烧伤）、db[1]=4 吸取分母。
    SoulMark sm(657, "K287 魂印", "test", EffectArgs(std::vector<int>{2, 4}));
    sm.monster_id = 2932;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "妮可萝", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_foe_precision(ScenarioBattle& b) {
    // 对手用精准（自身命中+1）：禁提升票在 → 整次失败等级不动；无票 → 正常 +1。
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4430 门开：吸取 + 禁提升（行为断言：对手回合内自我提升被整次拦下）═════
    {
        sbs_srand(4430);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        ctx.getPet(0).hp = 100000;   // 低体起测（吸取回血精确）
        round_foe_precision(b);
        CHECK_EQ(ctx.getPet(0).hp, 300000,
                 "T4430 657：吸取回血恰 1/4×800000=200000（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(ctx.getPet(1).hp <= kMaxHp - 200000,
              "T4430 657：对手掉血 ≥ 吸取量 200000（实测 -" +
                  std::to_string(kMaxHp - ctx.getPet(1).hp) +
                  "，含烧伤扣减点叠加故只断下界）");
        CHECK_EQ(ctx.ability_levels[1][5], 0,
                 "T4430 657：禁提升票在 → 对手精准（命中+1）整次失败（实测 lv=" +
                     std::to_string(ctx.ability_levels[1][5]) + "）");
        std::cout << "  [T4430] 门开吸取+禁提升 通过\n";
    }

    // ══ T4431 门闭：无吸取不禁提升 ═════════════════════════════════════════════
    {
        sbs_srand(4431);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_foe_precision(b);
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp,
                 "T4431 657：对手无烧伤 → 不吸取（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.ability_levels[1][5], 1,
                 "T4431 657：门闭 → 对手精准正常 +1（实测 lv=" +
                     std::to_string(ctx.ability_levels[1][5]) + "）");
        std::cout << "  [T4431] 门闭对照 通过\n";
    }

    // ══ T4432 门翻：烧伤到期 → 停止吸取 ════════════════════════════════════════
    {
        sbs_srand(4432);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/1, /*actor=*/0);
        idle_round(b);   // R1：带烧伤（吸取 + 烧伤扣减）
        const int foe_after_r1 = ctx.getPet(1).hp;
        CHECK(foe_after_r1 < kMaxHp,
              "T4432 657：R1 烧伤门开（吸取/扣减至少其一生效）");
        idle_round(b);   // R2：烧伤到期 → 不再吸取（无其它掉血源）
        CHECK_EQ(ctx.getPet(1).hp, foe_after_r1,
                 "T4432 657：R2 烧伤到期 → 停止吸取（实测变化 -" +
                     std::to_string(foe_after_r1 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T4432] 门翻停止 通过\n";
    }

    std::cout << "scenario_4430 PASS (K287 烧伤门禁提升+周期吸取族)" << std::endl;
    return 0;
}
