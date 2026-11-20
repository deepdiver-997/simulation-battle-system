// 场景 5720 —— J241 降态增伤族（858，moves_lib/j_negative_level_amp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5720-5729）
//
// 断言口径（快照 addPct 精确 + 伤害比值）：
//   - 载体 19048 噬神叹息斩 [180,858]|[10]（物 160）；180 清对手回合类效果，对手无
//     回合效果 → no-op，不污染本宏观测。
//   - 负等级放在**特攻/特防槽**（1/3）——物理刀伤害公式不读它们，伤害差异只来自 858
//     的增伤；排除"降防本身也加伤"的混杂。
//   - T5720.a：对手特攻 -2 → sum=2 → addPct == 20。
//   - T5720.b：对手特攻 -6 + 特防 -6 → sum=12 → addPct == 120，伤害 ≈2.2×（>1.5×）。
//   - T5720.c：对手只有**正**等级（攻 +2）→ sum=0 → addPct == 0（只数下降）。
//   - T5720.d：无任何等级 → addPct == 0。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/battleWorkspace.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40（填充刀）
constexpr int kAmpCarrier = 19048; // [180,858]|[10] 噬神叹息斩：物 160
constexpr int kMaxHp   = 99999;

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kAmpCarrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

struct AmpResult {
    int add_pct = 0;
    int damage = 0;
};

// 对手等级设置：(槽, 等级) 列表；空 = 无等级。
AmpResult run_case(const std::vector<std::pair<int, int>>& foe_levels) {
    ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    for (const auto& kv : foe_levels) {
        ctx.ability_levels[1][kv.first] = kv.second;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
    ctx.debug_step_mode = false;
    const DamageSnapshot& d = ctx.resolvedDamage;
    CHECK(d.attackerId == 0 && d.defenderId == 1 && d.isRed,
          "T5720 观测面为本次先手红伤快照");
    AmpResult r;
    r.add_pct = d.addPct;
    r.damage = d.final;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5720.a 对手特攻 -2 → sum=2 ×10% = 20% ══
    {
        const AmpResult r = run_case({{1, -2}});
        std::cout << "  [T5720.a] foe spa -2: addPct=" << r.add_pct
                  << " damage=" << r.damage << std::endl;
        CHECK_EQ(r.add_pct, 20, "T5720.a 对手特攻 -2 → sum=2 → addPct=20");
    }

    // ══ T5720.b 对手特攻/特防各 -6 → sum=12 ×10% = 120%，伤害 ≈2.2× ══
    {
        const AmpResult base = run_case({});
        const AmpResult r = run_case({{1, -6}, {3, -6}});
        std::cout << "  [T5720.b] foe spa/spd -6: addPct=" << r.add_pct
                  << " damage=" << r.damage << " | base addPct=" << base.add_pct
                  << " damage=" << base.damage << std::endl;
        CHECK_EQ(r.add_pct, 120, "T5720.b 对手 -6/-6 → sum=12 → addPct=120");
        CHECK(r.damage > base.damage * 15 / 10,
              "T5720.b 120% 增伤 → 伤害 >1.5×（实测 amp=" + std::to_string(r.damage) +
                  " base=" + std::to_string(base.damage) + "）");
    }

    // ══ T5720.c 只有正等级 → sum=0（只数下降）══
    {
        const AmpResult r = run_case({{0, 2}, {2, 1}});
        std::cout << "  [T5720.c] foe atk+2 def+1: addPct=" << r.add_pct
                  << " damage=" << r.damage << std::endl;
        CHECK_EQ(r.add_pct, 0, "T5720.c 对手仅正等级 → sum=0 → addPct=0");
    }

    // ══ T5720.d 无等级 → addPct=0 ══
    {
        const AmpResult r = run_case({});
        std::cout << "  [T5720.d] foe no levels: addPct=" << r.add_pct
                  << " damage=" << r.damage << std::endl;
        CHECK_EQ(r.add_pct, 0, "T5720.d 对手无等级 → addPct=0");
    }

    std::cout << "scenario_5720 PASS (858 对手下降等级总和×pct 增伤)" << std::endl;
    return 0;
}
