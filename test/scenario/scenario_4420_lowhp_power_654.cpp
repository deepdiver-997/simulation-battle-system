#include <utils/sbs_rand.h>
// 场景 4420 —— K286 低体门威力增伤族（654，soul_lib/k286_lowhp_power_amp.cpp）
//（2026-10-21，worktree ../agents/k286；号段 4420-4429 → T4420-T4423 四块）
//
// 断言口径（654「血量低于1/2时,技能威力提升40%」）：
//   - T4420 门开：体力低于 1/2 → 威力视图 ×140/100（35 → 49，逐位）。
//   - T4421 门闭（体力足）：体力 = max → 视图原值 35、rewritten 不置位。
//   - T4422 **边界**：体力**恰在** max/2 → 门闭（K245 798 直系口径：int 除法
//     **严格边界**，"低于1/2"不含等于）。另测 max/2 - 1 → 门开。
//   - T4423 门闭对照：不带魂印 → 视图原值、rewritten 不置位。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,40}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ 断言读 **ws.skill_power_view[0]**（K205 325 场景 3445 范式）——比伤害比精确
//   （伤害带 85%~100% 浮动，比值断言要放宽带；view 是逐位整数）。
// ⚠️ 停泊法：`step_past(State::BATTLE_FIRST_SKILL_EFFECT)` 后读视图（K205 同款）。
#include <iostream>
#include <utility>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：威力 35（物理攻击技）
constexpr int kMaxHp = 99999;

SoulMark make_mark() {
    // DB args 原序 [2,40]：db[0]=2（"1/2"分母）、db[1]=40（"40%"）——tips 字面自证。
    SoulMark sm(654, "K286 魂印", "test", EffectArgs(std::vector<int>{2, 40}));
    sm.monster_id = 2921;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "凯撒奥拉夫", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/300, /*atk=*/1, kMaxHp, make_mark())
        : make_pet(1, "裸凯撒奥拉夫", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 压体力 → 出手 → 停泊在 SKILL_EFFECT 后读威力视图。
std::pair<int, bool> probe(int force_hp, bool with_mark = true) {
    ScenarioBattle b(make_party(mark_pet(with_mark)), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261021);
    b.start();
    if (force_hp > 0) {
        ctx.seerRobot[0].elfPets[0].hp = force_hp;
    }
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    std::pair<int, bool> r{ctx.ws.skill_power_view[0].value,
                           ctx.ws.skill_power_view[0].rewritten};
    b.drive();
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4420 门开：体力低于 1/2 → 威力 ×140/100 ══
    {
        const int half = kMaxHp / 2;
        auto [view, rewritten] = probe(/*force_hp=*/half - 1);
        CHECK_EQ(view, 35 * 140 / 100,
                 "T4420 654：体力低于 1/2 → 技能威力 ×140/100（35 → 49，实测 "
                     + std::to_string(view) + "）");
        CHECK(rewritten, "T4420 654：rewritten 置位");
        std::cout << "  [T4420] 低体门开增伤 通过\n";
    }

    // ══ T4421 门闭：体力充足 → 原威力 ══
    {
        auto [view, rewritten] = probe(/*force_hp=*/kMaxHp);
        CHECK_EQ(view, 35, "T4421 654：体力充足 → 威力视图原值 35");
        CHECK(!rewritten, "T4421 654：体力充足 → rewritten 不置位（门闭）");
        std::cout << "  [T4421] 门闭对照 通过\n";
    }

    // ══ T4422 边界：恰在 max/2 → 门闭；max/2-1 → 门开 ══
    {
        const int half = kMaxHp / 2;
        auto [view_eq, rw_eq] = probe(/*force_hp=*/half);
        CHECK_EQ(view_eq, 35,
                 "T4422 654：体力恰在 1/2 → 门闭（'低于'不含等于；int 除法严格边界）");
        CHECK(!rw_eq, "T4422 654：体力恰在 1/2 → rewritten 不置位");
        auto [view_lt, rw_lt] = probe(/*force_hp=*/half - 1);
        CHECK_EQ(view_lt, 35 * 140 / 100,
                 "T4422 654：体力 1/2 - 1 → 门开（增伤生效）");
        CHECK(rw_lt, "T4422 654：体力 1/2 - 1 → rewritten 置位");
        std::cout << "  [T4422] 边界口径 通过\n";
    }

    // ══ T4423 无魂印对照 ══
    {
        const int half = kMaxHp / 2;
        auto [view, rewritten] = probe(/*force_hp=*/half - 1, /*with_mark=*/false);
        CHECK_EQ(view, 35, "T4423 654：对照——不带魂印时威力视图原值 35");
        CHECK(!rewritten, "T4423 654：对照——不带魂印时 rewritten 不置位");
        std::cout << "  [T4423] 无魂印对照 通过\n";
    }

    std::cout << "scenario_4420 PASS (K286 低体门威力增伤族)" << std::endl;
    return 0;
}
