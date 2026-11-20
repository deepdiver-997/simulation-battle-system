#include <utils/sbs_rand.h>
// 场景 4730 —— K317 先手攻击吸血+睡眠族（339，soul_lib/k317_first_lifesteal_sleep.cpp）
//（2026-10-21，worktree ../agents/k317；号段 4730-4739 → T4730-T4732 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102] → 吸血 20% = [17,20]；对手 NONE/对拍）：
//   - T4730 先手+必睡档（db[2]=100）：对手睡眠 + 自 hp = 100000 - 挠伤 + 吸血
//     ∈ [100010,100025]。
//   - T4731 后手门闭（印主速 100 vs 对手 200）：无吸血无睡眠（hp 只掉挠伤）。
//   - T4732 实参 15% 统计：30 刀对手睡眠回合 ∈ [1,12]（p=0.15/n=30 → 期望 4.5）。
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
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);

SoulMark make_mark(std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道）。
    SoulMark sm(339, "K317 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2695;
    return sm;
}

ElfPet mark_pet(std::vector<int> row, int speed) {
    ElfPet p = make_pet(1, "圣瞳·天蛇太祖", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/300, 800000, make_mark(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4730 先手+必睡档 ══════════════════════════════════════════════════════
    {
        sbs_srand(4730);
        ScenarioBattle b(make_party(mark_pet({1, 20, 100, 8}, /*speed=*/300)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        both_attack(b);
        CHECK(ctx.has_active_abnormal_status(1, kSleep),
              "T4730 339：先手攻击（db[2]=100）→ 对手睡眠");
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 100010 && hp <= 100025,
              "T4730 339：吸血 20%×[87,102] - 挠伤 → hp ∈ [100010,100025]（实测 " +
                  std::to_string(hp) + "）");
        std::cout << "  [T4730] 先手吸血+必睡 通过\n";
    }

    // ══ T4731 后手门闭 ═════════════════════════════════════════════════════════
    {
        sbs_srand(4731);
        ScenarioBattle b(make_party(mark_pet({1, 20, 100, 8}, /*speed=*/100)),
                         make_party(foe_pet(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        both_attack(b);
        const int hp = ctx.getPet(0).hp;
        CHECK(hp >= 99998 && hp <= 99999,
              "T4731 339：后手 → 门闭无吸血（实测 hp=" + std::to_string(hp) + "）");
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "T4731 339：后手 → 对手不睡");
        std::cout << "  [T4731] 后手门闭 通过\n";
    }

    // ══ T4732 实参 15% 统计带 ══════════════════════════════════════════════════
    {
        sbs_srand(4732);
        ScenarioBattle b(make_party(mark_pet({1, 20, 15, 8}, /*speed=*/300)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        int sleep_rounds = 0;
        for (int i = 0; i < 30; ++i) {
            both_attack(b);
            if (ctx.has_active_abnormal_status(1, kSleep)) {
                ++sleep_rounds;
            }
        }
        CHECK(sleep_rounds >= 1 && sleep_rounds <= 12,
              "T4732 339：30 刀 15% 睡眠回合 ∈ [1,12]（实测 " +
                  std::to_string(sleep_rounds) + "）");
        std::cout << "  [T4732] 15% 睡眠统计带 通过（实测 " << sleep_rounds << "）\n";
    }

    std::cout << "scenario_4730 PASS (K317 先手攻击吸血+睡眠族)" << std::endl;
    return 0;
}
