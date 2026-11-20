#include <utils/sbs_rand.h>
// 场景 4820 —— K326 受击转 HP+提升态免异常族（670，soul_lib/k326_hitdrain_boostimmune.cpp）
//（2026-10-21，worktree ../agents/k326；号段 4820-4829 → T4820-T4823 四块）
//
// 断言口径（对手拍打 atk=300 → 印主受红伤 [87,102] → 转 HP 40% = [34,40]）：
//   - T4820 受击转 HP：受击后净变化 = -红伤 + [34,40] ∈ [99925,99960]。
//   - T4821 免异常门开：对手提升态（直写速度槽）→ 白盒 is_immune=true（duration=2
//     兼顾 post-drive 读）+ 行为门 apply_anomaly 被拦。
//   - T4822 门闭：对手无提升 → apply_anomaly 正常落。
//   - T4823 boss 无效：受击不转 HP（只掉红伤）。
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [40,2,1]：db[0]=40 转化%（余废位）。
    SoulMark sm(670, "K326 魂印", "test", EffectArgs(std::vector<int>{40, 2, 1}));
    sm.monster_id = 2944;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "鬼剑骷髅", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
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

    // ══ T4820 受击转 HP ════════════════════════════════════════════════════════
    {
        sbs_srand(4820);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        foe_attack_round(b);
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 99925 && hp <= 99960,
              "T4820 670：受击 [87,102] 转 40% = [34,40] → 净 ∈ [99925,99960]（实测 " +
                  std::to_string(hp) + "）");
        std::cout << "  [T4820] 受击转 HP 通过\n";
    }

    // ══ T4821 免异常门开：对手提升态 → 票在 + apply 被拦 ═══════════════════════
    {
        sbs_srand(4821);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][4] = 1;   // 对手速度槽 +1 → 提升态
        foe_attack_round(b);            // R1 内 ROUND_START 已挂票（duration=2）
        CHECK(ctx.is_immune(0, ImmunityType::ANOMALY, State::BATTLE_ROUND_START),
              "T4821 670：白盒——ANOMALY 免疫票在（duration=2 窗内）");
        apply_anomaly(&ctx, 0, kBurn, /*duration_rounds=*/3, /*actor=*/1);
        CHECK(!ctx.has_active_abnormal_status(0, kBurn),
              "T4821 670：行为门——免疫票拦下烧伤施加");
        std::cout << "  [T4821] 提升态免异常 通过\n";
    }

    // ══ T4822 门闭：对手无提升 → 烧伤可落 ══════════════════════════════════════
    {
        sbs_srand(4822);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        apply_anomaly(&b.ctx(), 0, kBurn, /*duration_rounds=*/3, /*actor=*/1);
        CHECK(b.ctx().has_active_abnormal_status(0, kBurn),
              "T4822 670：门闭 → 烧伤正常施加");
        std::cout << "  [T4822] 门闭对照 通过\n";
    }

    // ══ T4823 boss 无效：受击不转 HP ═══════════════════════════════════════════
    {
        sbs_srand(4823);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        foe_attack_round(b);
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 99898 && hp <= 99913,
              "T4823 670：boss 无效 → 只掉红伤不转 HP（实测 hp=" +
                  std::to_string(hp) + "）");
        std::cout << "  [T4823] boss 无效门 通过\n";
    }

    std::cout << "scenario_4820 PASS (K326 受击转 HP+提升态免异常族)" << std::endl;
    return 0;
}
