// 场景 6330 —— J300 未暴击PP循环族（1689，moves_lib/j_nocrit_pp_cycle.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6330-6339）
//
// 断言口径：
//   - 1689（35071 火中生莲华 [1530,1483,1689]，1689=[1,1]）：行动末未暴击 →
//     自身所有技能 +1 PP、对手所有技能 −1 PP。
//   - 未暴击档用默认 crit=0；暴击档用 set_crit_sixteenths(16) 强制必暴（确定性）。
//   - PP 观测 = 逐槽绝对值（自身使用槽 max−1+1=max；对手每槽 max−1）。
//   - 审计槽 991689100（暴击旗）/991689200（恢复槽数）证明前提真的成立。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1689 = 35071;   // 火中生莲华 [1530,1483,1689]|[200,2,1,1] 特150
constexpr int kPaiDa = 10004;

constexpr int kCritKey = 991689100;      // +self
constexpr int kRestoreKey = 991689200;   // +self

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(bool force_crit) {
    ElfPet p = make_pet(1, "己方", {kCarrier1689, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    if (force_crit) {
        set_crit_sixteenths(p, 16);   // 必暴（确定性档）
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void run_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6330.a 未暴击：自回 1 PP，对手每槽 −1 ══
    {
        ScenarioBattle b(make_party(mk_self(/*force_crit=*/false)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_max = ctx.getPet(0).skills[0].maxPP;
        CHECK(self_max > 1, "T6330.a 前置：载体 PP 有限且 >1（实测 " +
                                std::to_string(self_max) + "）");
        run_round(b);
        CHECK_EQ(stor(ctx, kCritKey + 0), 0, "T6330.a 本回合未暴击（旗 0）");
        CHECK(stor(ctx, kRestoreKey + 0) > 0, "T6330.a 恢复原语确实变更了槽");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, self_max,
                 "T6330.a 自身使用槽：max−1+1 = max（实测 " +
                     std::to_string(ctx.getPet(0).skills[0].pp) + "）");
        for (int i = 0; i < 5; ++i) {
            const Skills& sk = ctx.getPet(1).skills[i];
            if (sk.pp < 0 || sk.maxPP < 0) {
                continue;
            }
            CHECK_EQ(sk.pp, sk.maxPP - 1,
                     "T6330.a 对手槽 " + std::to_string(i) + " = max−1");
        }
    }

    // ══ T6330.b 暴击：整条不执行（自身 PP 不回、对手 PP 不削）══
    {
        ScenarioBattle b(make_party(mk_self(/*force_crit=*/true)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_max = ctx.getPet(0).skills[0].maxPP;
        run_round(b);
        CHECK_EQ(stor(ctx, kCritKey + 0), 1, "T6330.b 本回合必暴（旗 1）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, self_max - 1,
                 "T6330.b 暴击 → 不回 PP（max−1，实测 " +
                     std::to_string(ctx.getPet(0).skills[0].pp) + "）");
        for (int i = 0; i < 5; ++i) {
            const Skills& sk = ctx.getPet(1).skills[i];
            if (sk.pp < 0 || sk.maxPP < 0) {
                continue;
            }
            CHECK_EQ(sk.pp, sk.maxPP, "T6330.b 暴击 → 对手槽 " + std::to_string(i) +
                                          " 不削");
        }
    }

    std::cout << "scenario 6330 OK" << std::endl;
    return 0;
}
