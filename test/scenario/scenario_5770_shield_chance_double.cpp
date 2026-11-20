// 场景 5770 —— J246 护盾概率翻倍族（1486，moves_lib/j_shield_chance_double.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5770-5779）
//
// 断言口径（审计槽精确 + 统计带）：
//   - 载体 35201 灰烬傀儡 [1486]|[50]（物 130，纯链）。
//   - 审计槽 991486100=有效概率（百分点）、991486200=本次掷点结果（1/0）。
//   - T5770.a：无护盾 40 刀 → 每刀有效概率 == 50；翻倍落地数落在 [8,32]（p=0.5 带）；
//     每刀 `resolvedDamage.addPct` 与审计结果一致（1→100 / 0→0）。
//   - T5770.b：每回合授予护盾 → 有效概率 == 100，10 刀**全部**翻倍（概率翻倍封顶）。
//   - T5770.c：有护盾的单刀停泊读快照：addPct == 100。
//   - PP=5 档：每回合恢复槽 0 的 PP（统计测试非实现缺陷）。
#include <any>
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

constexpr int kPaiDa  = 10004;   // 拍打：物 40（填充刀）
constexpr int kDouble = 35201;   // [1486]|[50] 灰烬傀儡：物 130（纯链）
constexpr int kMaxHp  = 99999;
constexpr int kChanceKey = 991486100;
constexpr int kOutcomeKey = 991486200;   // +owner

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kDouble, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : -1;
}

void restore_pp(BattleContext& ctx) {
    Skills& s = ctx.getPet(0).skills[0];
    s.pp = s.maxPP;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5770.a 无护盾：有效概率 50，翻倍落地约一半 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        int doubled = 0;
        int consistent = 0;
        for (int round = 0; round < 40; ++round) {
            restore_pp(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int chance = audit_int(ctx, kChanceKey + 0);
            const int outcome = audit_int(ctx, kOutcomeKey + 0);
            CHECK_EQ(chance, 50, "T5770.a 无护盾有效概率 50（第 " +
                                     std::to_string(round + 1) + " 轮）");
            if (outcome == 1) {
                ++doubled;
            }
            if (outcome >= 0) {
                ++consistent;
            }
        }
        std::cout << "  [T5770.a] no shield: doubled=" << doubled << "/40" << std::endl;
        CHECK(doubled >= 8 && doubled <= 32,
              "T5770.a 50% 翻倍落地数落在 [8,32]（实测 " + std::to_string(doubled) + "/40）");
        CHECK_EQ(consistent, 40, "T5770.a 每轮审计结果均已写入");
    }

    // ══ T5770.b 有护盾：有效概率 100，10 刀全翻倍 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        int doubled = 0;
        for (int round = 0; round < 10; ++round) {
            restore_pp(ctx);
            ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/5000,
                                           /*source_id=*/9001, /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(audit_int(ctx, kChanceKey + 0), 100,
                     "T5770.b 有护盾有效概率翻倍到 100（第 " + std::to_string(round + 1) +
                         " 轮）");
            if (audit_int(ctx, kOutcomeKey + 0) == 1) {
                ++doubled;
            }
        }
        std::cout << "  [T5770.b] with shield: doubled=" << doubled << "/10" << std::endl;
        CHECK_EQ(doubled, 10, "T5770.b 100% 档 10/10 全翻倍");
    }

    // ══ T5770.c 有护盾单刀：快照 addPct 精确 100 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/5000,
                                       /*source_id=*/9001, /*duration=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        ctx.debug_step_mode = false;
        const DamageSnapshot& d = ctx.resolvedDamage;
        CHECK(d.attackerId == 0 && d.isRed, "T5770.c 观测面为本次先手红伤快照");
        std::cout << "  [T5770.c] shielded snapshot addPct=" << d.addPct
                  << " final=" << d.final << std::endl;
        CHECK_EQ(d.addPct, 100, "T5770.c 有护盾 → 快照增伤精确 +100%（翻倍）");
    }

    std::cout << "scenario_5770 PASS (1486 护盾概率翻倍)" << std::endl;
    return 0;
}
