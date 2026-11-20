// 场景 4330 —— K277 回合回血低体翻倍族（610，soul_lib/k277_round_heal_double.cpp）
//（2026-10-21，worktree ../agents/k277；号段 4330-4339 → T4330-T4331 两块）
//
// 断言口径（610「每回合恢复1/5体力，体力少于1/3时恢复效果翻倍」）：
//   - T4330 翻倍门开：印主体力 10000（< 99999/3 = 33333）→ 回合开始恢复
//     **2 倍** = 2 × (99999/5) = 39998（压血到 10000 防满血封顶）。
//   - T4331 翻倍门闭：印主体力 60000（> 33333）→ 恢复 1 倍 = 99999/5 = 19999。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {5,3}（不补 {0,0} 前缀）——实现侧走
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
constexpr int kHealOnce = kMaxHp / 5;   // 19999（1/5）
constexpr int kHealDouble = kHealOnce * 2;   // 39998（翻倍）

// 印主：max 恒 99999；**当前体力**在开局后单独压（make_pet 的 hp 同时是
// 最大体力与初始体力 → 门限判据要用"当前体力 < max/3"，必须分开设）。
ElfPet mark_pet(std::vector<int> args = {5, 3}) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(610, "K277 魂印", "",
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

// 跑一回合，返回印主本回合回血量。cur_hp > 0 时在回合开始前把当前体力压到该值。
int one_round(ScenarioBattle& b, int cur_hp = 0) {
    BattleContext& ctx = b.ctx();
    if (cur_hp > 0) {
        ctx.getPet(0).hp = cur_hp;   // 压体力（max 不变）
    }
    const int hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.getPet(0).hp - hp0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4330 翻倍门开：体力 10000 < 99999/3 → 恢复 2 倍 ════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int healed = one_round(b, /*cur_hp=*/10000);   // 10000 < 99999/3 → 翻倍门开
        CHECK_EQ(healed, kHealDouble,
                 "T4330 610：体力低于 1/3 → 每回合恢复效果翻倍（2×99999/5=39998）");
        std::cout << "  [T4330] 低体翻倍 通过（healed=" << healed << "）\n";
    }

    // ══ T4331 翻倍门闭：体力 60000 > 99999/3 → 恢复 1 倍 ════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int healed = one_round(b, /*cur_hp=*/60000);   // 60000 > 99999/3 → 门闭
        CHECK_EQ(healed, kHealOnce,
                 "T4331 610：体力不低于 1/3 → 每回合恢复 1/5 体力（19999）");
        std::cout << "  [T4331] 门闭对照 通过（healed=" << healed << "）\n";
    }

    // 原序行参、严格阈值与先除后翻倍：99999/7*2=28570，而 99999*2/7=28571。
    struct HealCase {
        std::vector<int> args;
        int hp;
        int expected;
    };
    const std::vector<HealCase> cases{
        {{5, 3}, 33333, 19999},   // 阈值等号不翻倍
        {{5, 3}, 33332, 39998},   // 仅低 1 HP 即翻倍
        {{7, 4}, 10000, 28570},   // 两项非默认 + 整除顺序
        {{7, 4}, 30000, 14285},   // 改门限，不能仍按默认 1/3
        {{}, 10000, 39998},       // 缺参才回退
        {{7}, 30000, 28570},      // 仅缺门限
        {{0, 3}, 10000, 0},       // 显式零分母关闸，不回退
        {{5, 0}, 10000, 0},
    };
    for (const auto& c : cases) {
        ScenarioBattle b(make_party(mark_pet(c.args)), make_party(plain_pet()));
        b.start();
        CHECK_EQ(one_round(b, c.hp), c.expected,
                 "610：原序行参/严格阈值/缺失回退/显式零/先除后翻倍");
    }

    // 同号场下宿主故意给零参；实际节点必须读 host_slot=1 的行参。
    {
        auto party = make_party(plain_pet());
        party[0] = mark_pet({0, 3});
        party[1] = mark_pet({7, 4});
        ScenarioBattle b(party, make_party(plain_pet()));
        b.ctx().on_stage[0] = 1;
        b.start();
        CHECK_EQ(one_round(b, 30000), 14285, "610：行参来自实际 host_slot");
    }

    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        b.ctx().grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                               0, 0, 0);
        CHECK_EQ(one_round(b, 10000), 0, "610：低体翻倍仍受封回血约束");
    }

    // 通过 harness 停泊到真实 ROUND_START 再置死亡，不让后续死切掩盖错误复活。
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_until(State::BATTLE_ROUND_START);
        REQUIRE(ctx.currentState == State::BATTLE_ROUND_START);
        ctx.getPet(0).hp = 0;
        b.step();
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                 "610：ROUND_START 普通回血不复活已倒宿主");
    }

    std::cout << "scenario_4330 PASS (K277 回合回血低体翻倍族)" << std::endl;
    return 0;
}
