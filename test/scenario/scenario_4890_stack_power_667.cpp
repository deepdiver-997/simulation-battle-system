#include <utils/sbs_rand.h>
// 场景 4890 —— K333 累积威力提升族（667，soul_lib/k333_stack_power.cpp）
//（2026-10-21，worktree ../agents/k333；号段 4890-4899 → T4890-T4891 两块）
//
// 断言口径（667「每次使用伤害技能都能使自身技能威力提升5%,最高30%」）：
//   **字段断言**（停泊到 FIRST_SKILL_EFFECT 跑完读 ws.skill_power_view[0].value）
//   ——SOP §五.1：威力改写族的伤害比例断言不可对齐（rewritten 会重跑公式），
//   必须读威力视图字段（K9 场景 2861/2862 同款手法）。
//   - T4890 第 1 次使用：威力视图 = base × 105/100（层数 1 → +5%）。
//   - T4891 封顶：层数累积到 ≥6（6×5%=30）后，威力视图 = base × 130/100
//     （封顶 db[1]=30%，再多层不再涨）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {5,30}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击技载体

// 印主（可带/不带魂印）。
ElfPet mark_pet(bool with_mark) {
    SoulMark sm;
    if (with_mark) {
        sm = SoulMark(667, "K333 魂印", "", EffectArgs(std::vector<int>{5, 30}));
    }
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999, std::move(sm));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet dummy_pet() {
    ElfPet p = make_pet(2, "木桩", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/50, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 停泊到 FIRST_SKILL_EFFECT 跑完，读印主威力视图。
int probe_power_view(bool with_mark, int rounds) {
    ScenarioBattle b(make_party(mark_pet(with_mark)), make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261021);
    b.start();
    for (int r = 0; r < rounds; ++r) {
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        const int view = ctx.ws.skill_power_view[0].value;
        b.drive();
        if (r == rounds - 1) {
            return view;   // 最后一轮读值
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4890 第 1 次使用 → base × 105/100 ═══════════════════════════════════
    {
        const int base = probe_power_view(/*with_mark=*/false, /*rounds=*/1);
        const int v1 = probe_power_view(/*with_mark=*/true, /*rounds=*/1);
        CHECK(base > 0, "T4890 前提：对照组威力视图已物化");
        CHECK(v1 == base * 105 / 100,
              "T4890 667：第 1 次使用伤害技能 → 技能威力 +5%（层数 1）");
        std::cout << "  [T4890] 首层提升 通过（base=" << base << " v1=" << v1 << "）\n";
    }

    // ══ T4891 封顶：≥6 层后封在 30% ═════════════════════════════════════════
    {
        const int base = probe_power_view(/*with_mark=*/false, /*rounds=*/1);
        // 6 轮 → 层数 6 → 6×5% = 30% = 封顶值
        const int v6 = probe_power_view(/*with_mark=*/true, /*rounds=*/6);
        // 8 轮 → 层数 8 → 8×5% = 40% → 应封顶在 30%
        const int v8 = probe_power_view(/*with_mark=*/true, /*rounds=*/8);
        CHECK(v6 == base * 130 / 100,
              "T4891 667：6 层（6×5%=30%）→ 威力 +30%（到达上限）");
        CHECK(v8 == base * 130 / 100,
              "T4891 667：8 层（8×5%=40%）→ 仍封顶 +30%（不超上限）");
        std::cout << "  [T4891] 封顶 通过（v6=" << v6 << " v8=" << v8
                  << " base=" << base << "）\n";
    }

    std::cout << "scenario_4890 PASS (K333 累积威力提升族)" << std::endl;
    return 0;
}
