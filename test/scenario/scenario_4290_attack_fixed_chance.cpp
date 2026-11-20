#include <utils/sbs_rand.h>
// 场景 4290 —— K273 攻技概率固伤族（603，soul_lib/k273_attack_fixed_chance.cpp）
//（2026-10-21，worktree ../agents/k273；号段 4290-4299 → T4290-T4292 三块）
//
// 断言口径（拍打 atk=300；对手 atk=1 不还手、hp 池 800000）：
//   逐刀判别：红伤浮动带宽 ≈15%（远小于固伤 200）→ 单刀掉血 >150 即判"固伤触发刀"，
//   无需红伤绝对值锚。对手血池充裕不致死。
//   - T4290 db[1]=50：30 刀触发 ∈ [6,24]（p=0.5/n=30 → 期望 15，±9 覆盖 p<1e-4）。
//   - T4291 db[1]=0：20 刀零触发（关闸档，args 驱动直证）。
//   - T4292 db[1]=100：30 刀全触发（roll_percent(100) 短路必真）。
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

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道，K236 场景同款）。
    SoulMark sm(603, "K273 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2837;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "邪念魔尊", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 跑 N 刀，返回"掉血 >150"的刀数（红伤带 ≤150 且与 +200 触发带不相交）。
int trigger_count(ScenarioBattle& b, int rounds) {
    BattleContext& ctx = b.ctx();
    int n = 0;
    for (int i = 0; i < rounds; ++i) {
        const int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        if (hp0 - ctx.getPet(1).hp > 150) {
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

    // ══ T4290 db[1]=50：30 刀触发 ∈ [6,24] ═════════════════════════════════════
    {
        sbs_srand(20261022);
        ScenarioBattle b(make_party(mark_pet({1, 50, 200})), make_party(foe_pet()));
        b.start();
        const int n = trigger_count(b, 30);
        CHECK(n >= 6 && n <= 24,
              "T4290 603：30 刀 50% 固伤触发 ∈ [6,24]（实测 " + std::to_string(n) +
                  "）");
        std::cout << "  [T4290] 50% 固伤统计带 通过（实测 " << n << "）\n";
    }

    // ══ T4291 db[1]=0：20 刀零触发（关闸档）════════════════════════════════════
    {
        sbs_srand(4291);
        ScenarioBattle b(make_party(mark_pet({1, 0, 200})), make_party(foe_pet()));
        b.start();
        const int n = trigger_count(b, 20);
        CHECK_EQ(n, 0,
                 "T4291 603：db[1]=0 关闸 → 20 刀零触发（若误用固定 50% 会 ≈10）");
        std::cout << "  [T4291] 0% 关闸对照 通过\n";
    }

    // ══ T4292 db[1]=100：30 刀全触发（上限锚）══════════════════════════════════
    {
        sbs_srand(4292);
        ScenarioBattle b(make_party(mark_pet({1, 100, 200})), make_party(foe_pet()));
        b.start();
        const int n = trigger_count(b, 30);
        CHECK_EQ(n, 30,
                 "T4292 603：db[1]=100 → 30 刀全触发（实测 " + std::to_string(n) + "）");
        std::cout << "  [T4292] 100% 上限锚 通过（实测 " << n << "）\n";
    }

    std::cout << "scenario_4290 PASS (K273 攻技概率固伤族)" << std::endl;
    return 0;
}
