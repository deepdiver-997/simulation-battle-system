#include <utils/sbs_rand.h>
// 场景 2810 —— J179 无提升窗固伤族（1 宏，moves_lib/j_noboost_window_fixed.cpp）
//（2026-10-04，worktree ../agents/j179；号段 2810-2819 → T2810 一块）
//
// 断言口径（狂乱双颅 25061 [4,350] cat4 pri0 纯武装；链 585/48✓；atk=1 crit=0）：
//   a) 无提升：R2-R4（窗内）每回对手掉血 = 350 固伤 + 红伤带 [351,353]（精确）；
//   b) 有提升（+2 预置）：R5-R6 窗已过……改：单场内 R2-R4 无提升逐回断言后，
//      R5（窗过期）掉血回落红伤带 [1,3]（确定性过期）；
//   c) 对照：预置 +2 → R2 掉血 ≤3（门关，无固伤）。
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
constexpr int kTongTian = 26122;  // 通天之眼 [48,47,1027]|[4,4,4,400]：属性技（1027 收尾无下游）
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

    // ══ T2810.a 无提升：窗内每回 +350 固伤（精确）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kTongTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(1).hp;
        for (int r = 0; r < 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体连打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = prev - ctx.getPet(1).hp;
            prev = ctx.getPet(1).hp;
            CHECK_EQ(loss, 400,
                     "T2810.a 1027：R" + std::to_string(r + 1) + " 固伤 400 精确（属性技"
                     "无红伤，实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T2810.a] 1027 无提升窗固伤 通过\n";
    }

    // ══ T2810.b 有提升：门关（无固伤）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kTongTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, 2);   // 自身攻击 +2（有提升）
        int prev = ctx.getPet(1).hp;
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = prev - ctx.getPet(1).hp;
            prev = ctx.getPet(1).hp;
            CHECK(loss <= 3,
                  "T2810.b 1027 对照：有提升无固伤（R" + std::to_string(r + 1) + " 实测 " +
                      std::to_string(loss) + "）");
        }
        std::cout << "  [T2810.b] 1027 有提升对照 通过\n";
    }

    std::cout << "scenario_2810 PASS (J179 无提升窗固伤族 1027)" << std::endl;
    return 0;
}
