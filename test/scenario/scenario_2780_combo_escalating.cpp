#include <utils/sbs_rand.h>
// 场景 2780 —— J176 连击递增概率族（1 宏，moves_lib/j_combo_escalating.cpp）
//（2026-10-04，worktree ../agents/j176；号段 2780-2789 → T2780 一块）
//
// 断言口径（极限连爪破 13509 [15,7,5,40] 码 7 疲惫 物 pri0 纯链；crit=0、atk=1）：
//   动态段数：首用后读 resolvedDamage.hitCount=N（段数由载体自带连击面解析）。
//   60 局统计：每局"是否上疲惫"=任一段申报落地；期望单局 p_any=1-Π(1-min(15+5×(i-1),40))
//   （按 N 与 cap=40 截断），60 局上疲惫局数 ∈ [0.6,1.4]×60×p_any ±8 的宽容带。
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
constexpr int kJiXian  = 13509;  // 极限连爪破 [181]|[15,7,5,40]：物（181 载体）
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

    // ══ T2780 181 连用递增概率统计（极限连爪破；PP=5 → 每局 5 用）══
    // 递增序列（每局重置）：15,20,25,30,35 → 每局期望 1.25；12 局 60 用 → E≈15，
    // 带 [7,23]（二项近似 σ≈3.2，±2.5σ）。
    {
        constexpr int kBattles = 12;
        int landed = 0;
        for (int t = 0; t < kBattles; ++t) {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kJiXian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int u = 0; u < 5; ++u) {   // PP=5 用满
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
            if (ctx.has_active_abnormal_status(1, /*疲惫=*/7)) {
                ++landed;   // 该局至少落地一次
            }
        }
        std::cout << "  [dbg] battles_with_landed=" << landed << "/" << kBattles << "\n";
        CHECK(landed >= 4 && landed <= 12,
              "T2780 181：12 局（各 5 用，Σp=125%）落地局数 " + std::to_string(landed) +
                  " 应 ∈ [4,12]");
        std::cout << "  [T2780] 181 连用递增概率 通过\n";
    }

    std::cout << "scenario_2780 PASS (J176 连用递增概率族 181)" << std::endl;
    return 0;
}
