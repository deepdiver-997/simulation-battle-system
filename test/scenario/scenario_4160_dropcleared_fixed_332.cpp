// 场景 4160 —— K260 对手能力下降被解除→固伤族（332，soul_lib/k260_dropcleared_fixed.cpp）
//（2026-10-21，worktree ../agents/k260；号段 4160-4169 → T4160-T4162 三块）
//
// 断言口径（332「若对手能力下降被解除，则战斗阶段结束时直接给对手造成150点固定伤害」）：
//   - T4160 门开：回合首对手有负等级 → 回合内被解除（clear_stat_drops）→ 回合末
//     对手掉 150 固定体力（99999 满血，不浮动，精确）。
//   - T4161 门闭：回合首对手**无**下降 → 回合末不掉血。
//   - T4162 门闭：回合首对手有下降但回合末**仍有**下降（未被解除）→ 不掉血。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {150,0}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
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

constexpr int kJi = 10001;     // 撞击：攻击载体
constexpr int kMaxHp = 99999;
constexpr int kFixed = 150;     // db[0] = 固伤值

ElfPet mark_pet() {
    std::vector<int> args{150, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(332, "K260 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 停泊到 ROUND_START（快照已取）后，清掉对手的能力下降（模拟"被解除"）。
void clear_foe_drops_after_snapshot(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_ROUND_START);
    clear_stat_drops(&ctx, 1);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4160 门开：回合首有下降 → 回合内被解除 → 回合末固伤 150 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 回合首之前给对手压一级防御（负等级）→ 快照会记 had_drop。
        ctx.ability_levels[1][2] = -1;
        const int hp0 = ctx.getPet(1).hp;
        clear_foe_drops_after_snapshot(b);
        REQUIRE(ctx.ability_levels[1][2] == 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, hp0 - kFixed,
                 "T4160 332：对手能力下降被解除 → 回合末受到 150 点固定伤害");
        std::cout << "  [T4160] 门开固伤 通过\n";
    }

    // ══ T4161 门闭：回合首无下降 → 回合末不掉血 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        clear_foe_drops_after_snapshot(b);   // 无下降可清（clear 返回 0）
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T4161 332：对手本无能力下降 → 回合末无固伤（门闭）");
        std::cout << "  [T4161] 门闭对照 通过\n";
    }

    // ══ T4162 门闭：回合首有下降、回合末仍有 → 不掉血 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;
        const int hp0 = ctx.getPet(1).hp;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        // 不清 → 回合末仍有下降
        REQUIRE(ctx.ability_levels[1][2] < 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T4162 332：对手能力下降未被解除 → 回合末无固伤");
        std::cout << "  [T4162] 未解除对照 通过\n";
    }

    std::cout << "scenario_4160 PASS (K260 对手能力下降被解除→固伤族)" << std::endl;
    return 0;
}
