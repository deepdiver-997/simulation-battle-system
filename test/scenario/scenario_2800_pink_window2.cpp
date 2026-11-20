#include <utils/sbs_rand.h>
// 场景 2800 —— J178 窗粉伤族（2 宏，moves_lib/j_pink_window2.cpp）
//（2026-10-04，worktree ../agents/j178；号段 2800-2809 → T2800-T2801 两块）
//
// 断言口径（crit=0）：
//   - 853（王·毁灭星辰击 [20,10,40] 物 pri0；base=(max_hp 5000+速度面板)×pct）：
//     3 连打递增精确：20%→30%→40%（pct 钳 40 前不触顶），每打带红伤 [1,3]。
//   - 725（万世荒芜决 [3,30,3] 特 pri0 纯链；foe max 99999 → 每次粉 33333）：
//     20 局 × 3 回合（atk=1 红伤 1-2/回）：单回掉血 ≥30000 判 tick；
//     tick 回数 ∈ [10,26]（B(60,0.3) ±2.5σ）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kWangXing= 18331;  // 王·毁灭星辰击 [180,760,853]|[20,10,40]：物
constexpr int kWanShi  = 17072;  // 万世荒芜决 [725]|[3,30,3]：特（725 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2800 853 递增粉伤（王·毁灭星辰击）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kWangXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R2 ROUND_START 后 battle_attrs 已同步，速度面板可读
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int base = 5000 + ctx.ws.battle_attrs[0][NumericalPropertyIndex::SPEED];
        int prev = ctx.getPet(1).hp;
        const int expect[3] = {base * 20 / 100, base * 30 / 100, base * 40 / 100};
        for (int r = 0; r < 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = prev - ctx.getPet(1).hp;
            prev = ctx.getPet(1).hp;
            CHECK(loss >= expect[r] + 1 && loss <= expect[r] + 3,
                  "T2800 853：第 " + std::to_string(r + 1) + " 打粉 " +
                      std::to_string(expect[r]) + "+红伤带（实测 " +
                      std::to_string(loss) + "）");
        }
        std::cout << "  [T2800] 853 递增粉伤 通过（base=" << base << "）\n";
    }

    // ══ T2801 725 概率粉窗（万世荒芜决）══
    {
        constexpr int kBattles = 20;
        constexpr int kRounds = 3;
        int ticks = 0;
        for (int t = 0; t < kBattles; ++t) {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kWanShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            int prev = ctx.getPet(1).hp;
            for (int r = 0; r < kRounds; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int loss = prev - ctx.getPet(1).hp;
                if (loss >= 30000) {
                    ++ticks;   // 33333 ± 红伤
                }
                prev = ctx.getPet(1).hp;
            }
        }
        CHECK(ticks >= 10 && ticks <= 26,
              "T2801 725：60 回合 tick 数 " + std::to_string(ticks) +
                  " 应 ∈ [10,26]（B(60,0.3) ±2.5σ）");
        std::cout << "  [T2801] 725 概率粉窗 通过（tick " << ticks << "/60）\n";
    }

    std::cout << "scenario_2800 PASS (J178 窗粉伤族 853/725)" << std::endl;
    return 0;
}
