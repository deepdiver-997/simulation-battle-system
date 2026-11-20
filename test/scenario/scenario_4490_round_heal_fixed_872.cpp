// 场景 4490 —— K293 回合回血+等量固伤族（872，soul_lib/k293_round_heal_fixed.cpp）
//（2026-10-21，worktree ../agents/k293；号段 4490-4499 → T4490-T4492 三块）
//
// 断言口径（872「每回合恢复250点体力并造成等量固定伤害」）：
//   - T4490 门开：过 ROUND_START → 自身回血 +250（精确）、对手受 250 点固定伤害
//     （精确——FIXED 档粉伤**无浮动**，不像红伤有 85%~100% 浮动）。
//   - T4491 门闭对照：不带魂印 → 自身不回血、对手不掉血。
//   - T4492 **每回合生效**：连续两回合 → 自身累计回血 +500、对手累计掉 500。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {250,0}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **回血需要空间**：满血时回血不涨体力 → 先把印主压到缺口 1000 再过桶，
//   回血 250 后体力 = 起始+250（精确）。对手满血（99999）承接固伤无妨。
// ⚠️ 固伤是 **FIXED 档粉伤**（吃护罩/粉伤免减，不吃护盾——K233 844 口径），本场景
//   对手无护罩无免减 → 掉血恰为 250。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler（不出手）
constexpr int kMaxHp = 99999;
constexpr int kAmount = 250;   // db[0]：恢复量 = 等量固伤

SoulMark make_mark() {
    // DB args 原序 [250,0]：db[0]=250（tips 字面自证）；db[1]=0 不消费（"等量"）。
    SoulMark sm(872, "K293 魂印", "test", EffectArgs(std::vector<int>{250, 0}));
    sm.monster_id = 3260;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "暗地魔影·格莱奥丹", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/100, /*atk=*/1, kMaxHp, make_mark())
        : make_pet(1, "裸暗地魔影", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 压印主体力留回血空间 → 推进**一个完整回合** → 返回 [自身回血量, 对手掉血量]。
// ⚠️ **必须用 drive() 跑完整回合，不能用 step_past(ROUND_START)**：step_past 只在
//    `currentState <= target` 时步进，第 2 次调用时 currentState 已越过 ROUND_START
//    → 循环体不执行 → 第 2 回合的桶根本没跑（实测 roundCount 停在 0、healed=0）。
//    drive() = fsm_.run() 跑完当前回合（K 系多回合场景标准写法，如 scenario_956）。
// ⚠️ **只有第 1 回合压体力**——若每回合都重置为同一缺口，自身回血量会被重置吃掉
//    （实测 T4492 累计只有 250 而非 500：第 2 回合又从 kMaxHp-1000 起算）。
//    对手掉血是累积的（不受重置影响），故只有自身侧需要"只压一次"。
std::pair<int, int> one_round(ScenarioBattle& b, bool press_hp = true) {
    BattleContext& ctx = b.ctx();
    if (press_hp) {
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp - 1000;   // 留 1000 缺口
    }
    const int self_before = ctx.getPet(0).hp;
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return {ctx.getPet(0).hp - self_before, foe_before - ctx.getPet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4490 门开：自身回血 250 + 对手固伤 250 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        auto [healed, foe_lost] = one_round(b);
        CHECK_EQ(healed, kAmount,
                 "T4490 872：每回合恢复 250 点体力（实测 " + std::to_string(healed) + "）");
        CHECK_EQ(foe_lost, kAmount,
                 "T4490 872：对手受等量 250 点固定伤害（实测 "
                     + std::to_string(foe_lost) + "）");
        std::cout << "  [T4490] 门开回血+固伤 通过\n";
    }

    // ══ T4491 门闭对照：不带魂印 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(plain_pet()));
        b.start();
        auto [healed, foe_lost] = one_round(b);
        CHECK_EQ(healed, 0, "T4491 872：对照——不带魂印时不回血");
        CHECK_EQ(foe_lost, 0, "T4491 872：对照——不带魂印时对手不掉血");
        std::cout << "  [T4491] 门闭对照 通过\n";
    }

    // ══ T4492 每回合生效：两回合累计 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        int total_heal = 0;
        int total_foe = 0;
        for (int r = 0; r < 2; ++r) {
            auto [healed, foe_lost] = one_round(b, /*press_hp=*/r == 0);
            total_heal += healed;
            total_foe += foe_lost;
        }
        CHECK_EQ(total_heal, kAmount * 2,
                 "T4492 872：两回合累计回血 500（实测 " + std::to_string(total_heal) + "）");
        CHECK_EQ(total_foe, kAmount * 2,
                 "T4492 872：两回合累计固伤 500（实测 " + std::to_string(total_foe) + "）");
        std::cout << "  [T4492] 每回合生效 通过\n";
    }

    std::cout << "scenario_4490 PASS (K293 回合回血+等量固伤族)" << std::endl;
    return 0;
}
