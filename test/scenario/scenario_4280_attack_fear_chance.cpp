#include <utils/sbs_rand.h>
// 场景 4280 —— K272 攻技概率害怕族（395，soul_lib/k272_attack_fear_chance.cpp）
//（2026-10-21，worktree ../agents/k272；号段 4280-4289 → T4280-T4283 四块）
//
// 断言口径（拍打 atk=300；对手 atk=1 不还手、hp 池 800000）：
//   - T4280 db[2]=10：30 刀害怕回合数 ∈ [1,12]（p=0.1/n=30 → 期望 3，种子定序；
//     种子 20261022 为 K269 勘误后的居中档）。
//   - T4281 db[2]=0：20 刀零害怕（关闸档，args 驱动直证）。
//   - T4282 db[2]=100：30 刀害怕回合数 ≥25（上限锚；害怕无命中即醒类清除）。
//   - T4283 boss 无效：is_boss_challenge + db[2]=100 → 20 刀零害怕（手动门直证）。
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
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道，K236 场景同款）。
    SoulMark sm(395, "K272 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2821;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "天石灵尊", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, 800000, make_mark(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int fear_rounds(ScenarioBattle& b, int rounds) {
    BattleContext& ctx = b.ctx();
    int n = 0;
    for (int i = 0; i < rounds; ++i) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        if (ctx.has_active_abnormal_status(1, kFear)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4280 db[2]=10：30 刀害怕回合数 ∈ [1,12] ═══════════════════════════════
    {
        sbs_srand(20261022);
        ScenarioBattle b(make_party(mark_pet({1, 1, 10, 6})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 30);
        CHECK(n >= 1 && n <= 12,
              "T4280 395：30 刀 10% 害怕回合数 ∈ [1,12]（实测 " + std::to_string(n) +
                  "）");
        std::cout << "  [T4280] 10% 害怕统计带 通过（实测 " << n << "）\n";
    }

    // ══ T4281 db[2]=0：20 刀零害怕（关闸档）════════════════════════════════════
    {
        sbs_srand(4281);
        ScenarioBattle b(make_party(mark_pet({1, 1, 0, 6})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 20);
        CHECK_EQ(n, 0,
                 "T4281 395：db[2]=0 关闸 → 20 刀零害怕（若误用固定 10% 会 ≈2）");
        std::cout << "  [T4281] 0% 关闸对照 通过\n";
    }

    // ══ T4282 db[2]=100：30 刀害怕回合数 ≥25（上限锚）══════════════════════════
    {
        sbs_srand(4282);
        ScenarioBattle b(make_party(mark_pet({1, 1, 100, 6})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 30);
        CHECK(n >= 25,
              "T4282 395：db[2]=100 → 30 刀害怕回合数 ≥25（实测 " +
                  std::to_string(n) + "）");
        std::cout << "  [T4282] 100% 上限锚 通过（实测 " << n << "）\n";
    }

    // ══ T4283 boss 无效：boss 挑战 + 100% 档 → 零害怕 ══════════════════════════
    {
        sbs_srand(4283);
        ScenarioBattle b(make_party(mark_pet({1, 1, 100, 6})), make_party(foe_pet()));
        b.start();
        b.ctx().is_boss_challenge = true;
        const int n = fear_rounds(b, 20);
        CHECK_EQ(n, 0,
                 "T4283 395：boss 无效子句 → 20 刀零害怕（手动门直证）");
        std::cout << "  [T4283] boss 无效门 通过\n";
    }

    std::cout << "scenario_4280 PASS (K272 攻技概率害怕族)" << std::endl;
    return 0;
}
