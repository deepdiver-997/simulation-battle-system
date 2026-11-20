// 场景 2250 —— J139 非暴击吸取族（1 宏，moves_lib/j_noncrit_drain.cpp）
//（2026-10-04，worktree ../agents/j139；号段 2250-2259 → T2250 一块三例）
//
// 断言口径（红伤带 = 0.84×P×A/D；kMaxHp=800000 → 吸取 1/4=200000）：
//   - make_pet 默认清零暴击率（rate=0 短路不必暴）→ 全场非暴击=844 门恒开，吸取
//     200000 确定触发；载体 18294 戮血新月斩 [496,844]|[4]——496（暴击回满）无暴击
//     时休眠，链良性；
//   - a 非暴击吸取：R1 自方吃对手大刀（atk=720000 → 自伤 [205632,241920]，为 R2
//     回吸留足缺口防 clamp）→ R2 自方载体：对手恰掉 200000+红伤带、自身恰回 200000；
//   - b 暴击不吸：set_crit_sixteenths(16) 必暴 → 844 门关（对手只吃红伤带）；
//     同局 496 暴击回满自体（链良性，不碍断言）；
//   - c miss 门：accuracy=0 必 miss → 无红伤无吸取。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLuXue  = 18294;  // 戮血新月斩 [496,844]|[4]：物160（844 分母 4）
constexpr int kPaiDa  = 10004;  // 拍打：物40

constexpr int kMaxHp = 800000;  // 吸取 1/4 = 200000 整

void accuracy_set(ElfPet& p, int acc, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = acc;
        s.must_hit = must_hit ? 1 : 0;
    }
}

ElfPet self_pet(int acc, bool must_hit, bool force_crit) {
    ElfPet p = make_pet(1, "己方", {kLuXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    accuracy_set(p, acc, must_hit);
    if (force_crit) {
        set_crit_sixteenths(p, 16);
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_set(p, 100, false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2250.a 非暴击 → 吸取对手 1/4（先吃大刀留回吸缺口）══════════════════
    {
        ScenarioBattle b(make_party(self_pet(100, true, false)), make_party(foe_pet(720000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R1 自方挨刀
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
        b.drive();
        const int hp1_self = ctx.getPet(0).hp;
        const int taken1 = kMaxHp - hp1_self;
        CHECK(taken1 >= 205632 && taken1 <= 241920,
              "T2250.a 前置：R1 承伤带留足缺口（实测 " + std::to_string(taken1) + "）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 自方载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 200343 && foe_loss <= 200403,
              "T2250.a 844：对手掉红伤带+恰 200000（实测 " + std::to_string(foe_loss) + "）");
        const int self_gain = ctx.getPet(0).hp - hp1_self;
        CHECK_EQ(self_gain, 200000,
                 "T2250.a 844：自身恰回 200000（实测 " + std::to_string(self_gain) + "）");
    }
    // ══ T2250.b 必暴 → 门关不吸（同局 496 暴击回满自体=链良性）════════════════
    {
        ScenarioBattle b(make_party(self_pet(100, true, true)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自方载体（必暴）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 686 && foe_loss <= 806,
              "T2250.b 844：暴击局只吃暴击红伤带（×2）无吸取（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2250.c miss 门：accuracy=0 必 miss → 无红伤无吸取 ═══════════════════
    {
        ScenarioBattle b(make_party(self_pet(0, false, false)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss, 0,
                 "T2250.c 844：miss 无红伤无吸取（实测 " + std::to_string(foe_loss) + "）");
    }

    std::cout << "scenario_2250 PASS (J139 非暴击吸取族 844)" << std::endl;
    return 0;
}
