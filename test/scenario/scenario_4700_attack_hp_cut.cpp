#include <utils/sbs_rand.h>
// 场景 4700 —— K314 攻技削现血族（371，soul_lib/k314_attack_hp_cut.cpp）
//（2026-10-21，worktree ../agents/k314；号段 4700-4709 → T4700-T4703 四块）
//
// 断言口径（拍打 atk=300 → 红伤带 [87,102]；max=800000 → 33% 削 = 264000；对手 NONE）：
//   - T4700 db[1]=100：一刀 → 对手 = 800000-264000-红伤 ∈ [535890,535920]。
//   - T4701 db[1]=0：关闸 → 只红伤 [799898,799913]。
//   - T4702 db[1]=50：15 刀（触发判别 = 单刀掉血 >1000）触发 ∈ [3,13]
//     （p=0.5/n=15 → 期望 7.5；带 ±5 覆盖 p<1e-3）；存活门内循环。
//   - T4703 boss 无效：100% 档 → 只红伤。
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
constexpr int kMaxHp = 800000;

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道）。
    SoulMark sm(371, "K314 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2784;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "苍穹圣虎", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4700 100% 档：一刀 = 264000 削 + 红伤 ═════════════════════════════════
    {
        sbs_srand(4700);
        ScenarioBattle b(make_party(mark_pet({1, 100, 33, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 264087 && drop <= 264102,
              "T4700 371：100% → 削 264000+红伤（实测 " + std::to_string(drop) + "）");
        std::cout << "  [T4700] 100% 削现血 通过\n";
    }

    // ══ T4701 0% 关闸：只红伤 ══════════════════════════════════════════════════
    {
        sbs_srand(4701);
        ScenarioBattle b(make_party(mark_pet({1, 0, 33, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102,
              "T4701 371：0% 关闸 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4701] 0% 关闸对照 通过\n";
    }

    // ══ T4702 50% 统计带：15 刀触发 ∈ [3,13] ═══════════════════════════════════
    {
        sbs_srand(4702);
        ScenarioBattle b(make_party(mark_pet({1, 50, 33, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int triggers = 0;
        for (int i = 0; i < 15 && ctx.getPet(1).hp > 0; ++i) {
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (foe0 - ctx.getPet(1).hp > 1000) {
                ++triggers;   // 红伤 ≤102，触发刀掉血 ≥ 千级
            }
        }
        CHECK(triggers >= 3 && triggers <= 13,
              "T4702 371：15 刀 50% 触发 ∈ [3,13]（实测 " + std::to_string(triggers) +
                  "）");
        std::cout << "  [T4702] 50% 统计带 通过（实测 " << triggers << "）\n";
    }

    // ══ T4703 boss 无效：100% 档只红伤 ═════════════════════════════════════════
    {
        sbs_srand(4703);
        ScenarioBattle b(make_party(mark_pet({1, 100, 33, 1})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = foe0 - ctx.getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102,
              "T4703 371：boss 无效 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4703] boss 无效门 通过\n";
    }

    std::cout << "scenario_4700 PASS (K314 攻技削现血族)" << std::endl;
    return 0;
}
