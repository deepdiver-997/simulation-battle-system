// 场景 5650 —— J234 残血威力线性族（100，moves_lib/j_low_hp_power_linear.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5650-5659）
//
// 断言口径（停泊读 skill_power_view；坑集 #6）：
//   - 100（致命扭曲 18351 纯链 [100]，基础威力 150，max_hp=10000）：
//     view = 150 + 150 × 已损失体力 / 10000。
//   - a) 满血：view=150、rewritten=false（不改写）。
//   - b) 半血 5000：view=225（+75）、rewritten=true。
//   - c) 残血 2500：view=262（+112，int 除法逐位）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;  // 拍打：物 40（填充刀）
constexpr int kZhiMing = 18351; // [100]（纯链 pow=150）
constexpr int kBasePower = 150;
constexpr int kMaxHp = 10000;

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kZhiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

struct ViewProbe {
    int value = -1;
    bool rewritten = false;
};

// 压当前体力 → 出手 → 停泊在 SKILL_EFFECT 后读威力视图 → 跑完本回合。
ViewProbe probe_power(ScenarioBattle& b, BattleContext& ctx, int hp) {
    ctx.getPet(0).hp = hp;   // 只压当前体力，最大体力不变
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    ViewProbe r;
    r.value = static_cast<int>(ctx.ws.skill_power_view[0]);
    r.rewritten = ctx.ws.skill_power_view[0].rewritten;
    b.drive();
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5650 100 残血威力线性增长 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        const ViewProbe full = probe_power(b, ctx, /*hp=*/kMaxHp);
        CHECK_EQ(full.value, kBasePower, "T5650.a 100：满血威力 150（不改写）");
        CHECK(!full.rewritten, "T5650.a 100：满血不触发变威力标记");

        const ViewProbe half = probe_power(b, ctx, /*hp=*/5000);
        CHECK_EQ(half.value, kBasePower + kBasePower * 5000 / kMaxHp,
                 "T5650.b 100：半血威力 225（150+75，实测 " +
                     std::to_string(half.value) + "）");
        CHECK(half.rewritten, "T5650.b 100：残血走变威力改写（rewritten）");

        const ViewProbe low = probe_power(b, ctx, /*hp=*/2500);
        CHECK_EQ(low.value, kBasePower + kBasePower * 7500 / kMaxHp,
                 "T5650.c 100：残血威力 262（150+112，实测 " +
                     std::to_string(low.value) + "）");
        CHECK(low.rewritten, "T5650.c 100：残血变威力标记保持");

        std::cout << "  [T5650] 100 满血 150 / 半血 225 / 残血 262 通过\n";
    }

    std::cout << "scenario_5650 PASS (J234 残血威力线性族)" << std::endl;
    return 0;
}
