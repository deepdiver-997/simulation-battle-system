// 场景 3770 —— K233 双防和固定伤+回血族（844，soul_lib/k233_dualdef_fixed_heal.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3770-3779 → T3770 一块四例）
//
// 断言口径：make_pet 双方面板 def=特防=100 → 双防和=200 精确已知：
//   row1 附加/回复各 100、row2 各 120（等式断言）。
//   - a row1：己方攻击命中 → 自身回复恰 +100（预压血量防满血封顶）；
//     对手掉血 = 红伤(62~99 浮动带) + 100 ∈ (160,200)。
//   - b row2：回复恰 +120；对手掉血 ∈ (180,220)。
//   - c 无攻击回合：双段都不触发（血量原地）。
//   - d 属性技回合：无红伤管线 → watcher 静默（不回血不附加）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kGaoSu = 20017;  // 高速移动：属性技 filler（2830 实证可用）

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(844, "K233 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3227;   // 月之子·奇拉美
    return sm;
}

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, 99999, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{50, 50, 1};
    const std::vector<int> row2{60, 60, 1};

    // ══ T3770.a row1：攻击 → 附加 100 + 回复 100 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // 预压血量（满血回复会封顶）
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 100, "T3770.a 回复 = 自身双防和 200×50%");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 185 && foe_delta < 205,
              "T3770.a 对手掉血 = 红伤带 + 附加 100");
        std::cout << "  [T3770.a] row1 攻击双附加 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3770.b row2：附加 120 + 回复 120 ══
    {
        ScenarioBattle b(make_party(mark_pet(row2)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 120, "T3770.b 回复 = 自身双防和 200×60%");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 205 && foe_delta < 225,
              "T3770.b 对手掉血 = 红伤带 + 附加 120");
        std::cout << "  [T3770.b] row2 攻击双附加 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3770.c 无攻击回合：双段静默 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3770.c 无攻击不回复");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T3770.c 无攻击不附加");
        std::cout << "  [T3770.c] 无攻击静默 通过\n";
    }

    // ══ T3770.d 属性技回合：无红伤管线 → 静默 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3770.d 属性技不回血");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T3770.d 属性技不附加");
        std::cout << "  [T3770.d] 属性技静默 通过\n";
    }

    std::cout << "scenario_3770 PASS (K233 双防和固定伤+回血族)" << std::endl;
    return 0;
}
