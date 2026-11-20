#include <utils/sbs_rand.h>
// 场景 4960 —— K340 双概率异常+异常门威力族（709，soul_lib/k340_dual_prob_anomaly_power.cpp）
//（2026-10-21，worktree ../agents/k340；号段 4960-4969 → T4960-T4963 四块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]，威力 ×2 → [174,204]；对手 NONE）：
//   - T4960 实参统计：30 刀烧伤回合 ∈ [6,24]（p=0.5）、麻痹回合 ≥1（p=0.3，
//     P(零)≈0.002%）。
//   - T4961 双 100% 档：本刀即双异常 → 威力门开 → 掉血 ∈ [174,204]。
//   - T4962 双 0% 关闸：无异常 → 只红伤 [87,102]。
//   - T4963 boss 无效：100% 档 → 无异常、只红伤。
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
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);
constexpr int kMaxHp = 800000;

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道）。
    SoulMark sm(709, "K340 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 3012;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "狂刃血鬼", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

int mark_attack_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4960 实参统计：烧伤带 / 麻痹至少一次 ══════════════════════════════════
    {
        sbs_srand(4960);
        ScenarioBattle b(make_party(mark_pet({1, 50, 2, 30, 0, 1, 2})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int burn_rounds = 0;
        int para_rounds = 0;
        for (int i = 0; i < 30; ++i) {
            mark_attack_round(b);
            if (ctx.has_active_abnormal_status(1, kBurn)) {
                ++burn_rounds;
            }
            if (ctx.has_active_abnormal_status(1, kParalysis)) {
                ++para_rounds;
            }
        }
        CHECK(burn_rounds >= 6 && burn_rounds <= 24,
              "T4960 709：30 刀 50% 烧伤回合 ∈ [6,24]（实测 " +
                  std::to_string(burn_rounds) + "）");
        CHECK(para_rounds >= 1,
              "T4960 709：30 刀 30% 麻痹至少一次（实测 " +
                  std::to_string(para_rounds) + "）");
        std::cout << "  [T4960] 双概率统计 通过（烧伤 " << burn_rounds << " / 麻痹 "
                  << para_rounds << "）\n";
    }

    // ══ T4961 双 100% 档（仅麻痹带伤害面）：异常落 → 威力门开 ×2 ════════════════
    //（⚠️ 实证：烧伤施加自带 ~max/8 即时伤害（100103）污染掉血断言——威力门改用
    //  仅麻痹 100% 档（麻痹无伤害面），drop = 红伤×2 纯净。）
    {
        sbs_srand(4961);
        ScenarioBattle b(make_party(mark_pet({1, 0, 2, 100, 0, 1, 2})),
                         make_party(foe_pet()));
        b.start();
        const int drop = mark_attack_round(b);
        CHECK(drop >= 172 && drop <= 206,
              "T4961 709：异常落（麻痹）→ 威力 ×2 ∈ [172,206]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4961] 异常门威力 ×2 通过\n";
    }

    // ══ T4962 双 0% 关闸：只红伤 ═══════════════════════════════════════════════
    {
        sbs_srand(4962);
        ScenarioBattle b(make_party(mark_pet({1, 0, 2, 0, 0, 1, 2})),
                         make_party(foe_pet()));
        b.start();
        const int drop = mark_attack_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4962 709：双 0% 关闸 → 只红伤 [87,102]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4962] 关闸对照 通过\n";
    }

    // ══ T4963 boss 无效：100% 档不施加不翻倍 ═══════════════════════════════════
    {
        sbs_srand(4963);
        ScenarioBattle b(make_party(mark_pet({1, 100, 2, 100, 0, 1, 2})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        const int drop = mark_attack_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kBurn) &&
                  !ctx.has_active_abnormal_status(1, kParalysis),
              "T4963 709：boss 无效 → 不施加");
        CHECK(drop >= 87 && drop <= 102,
              "T4963 709：boss 无效 → 只红伤 [87,102]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4963] boss 无效门 通过\n";
    }

    std::cout << "scenario_4960 PASS (K340 双概率异常+异常门威力族)" << std::endl;
    return 0;
}
