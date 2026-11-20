// 场景 5800 —— J249 满体概率增伤族（1525，moves_lib/j_full_hp_chance_amp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5800-5809）
//
// 断言口径（审计槽精确 + 统计带 + 快照字段）：
//   - 载体 33836 冥牙嗜 [950,1525]|[2,50,100]（物 85；950 清回合效果，无则 no-op）。
//   - 审计槽 991525100=有效概率、991525200=掷点结果（1/0）。
//   - T5800.a：每回合开始满体力（99999/99999）→ 有效概率 100 → 10/10 全增伤。
//   - T5800.b：每回合开始半血（50000/99999）→ 有效概率 50 → 40 刀落地数在 [8,32]。
//   - T5800.c：满体单刀停泊读快照 → addPct == 100。
//   - 每回合重置 hp/PP（统计测试口径；非实现缺陷）。
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
constexpr int kAmp    = 33836;   // [950,1525]|[2,50,100] 冥牙嗜：物 85
constexpr int kMaxHp  = 99999;
constexpr int kChanceKey = 991525100;
constexpr int kOutcomeKey = 991525200;   // +owner

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kAmp, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

void prepare_round(BattleContext& ctx, int self_hp) {
    ctx.getPet(0).hp = self_hp;
    Skills& s = ctx.getPet(0).skills[0];
    s.pp = s.maxPP;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5800.a 满体 → 有效概率 100，10/10 增伤 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        int landed = 0;
        for (int round = 0; round < 10; ++round) {
            prepare_round(ctx, kMaxHp);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(audit_int(ctx, kChanceKey + 0), 100,
                     "T5800.a 满体有效概率翻倍到 100（第 " + std::to_string(round + 1) +
                         " 轮）");
            if (audit_int(ctx, kOutcomeKey + 0) == 1) {
                ++landed;
            }
        }
        std::cout << "  [T5800.a] full hp: landed=" << landed << "/10" << std::endl;
        CHECK_EQ(landed, 10, "T5800.a 满体 100% 档 10/10 增伤");
    }

    // ══ T5800.b 半血 → 有效概率 50，统计带 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        int landed = 0;
        for (int round = 0; round < 40; ++round) {
            prepare_round(ctx, kMaxHp / 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(audit_int(ctx, kChanceKey + 0), 50,
                     "T5800.b 半血有效概率 50（第 " + std::to_string(round + 1) + " 轮）");
            if (audit_int(ctx, kOutcomeKey + 0) == 1) {
                ++landed;
            }
        }
        std::cout << "  [T5800.b] half hp: landed=" << landed << "/40" << std::endl;
        CHECK(landed >= 8 && landed <= 32,
              "T5800.b 50% 落地数落在 [8,32]（实测 " + std::to_string(landed) + "/40）");
    }

    // ══ T5800.c 满体单刀：快照 addPct 精确 100 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        prepare_round(ctx, kMaxHp);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        ctx.debug_step_mode = false;
        const DamageSnapshot& d = ctx.resolvedDamage;
        CHECK(d.attackerId == 0 && d.isRed, "T5800.c 观测面为本次先手红伤快照");
        std::cout << "  [T5800.c] full-hp snapshot addPct=" << d.addPct
                  << " final=" << d.final << std::endl;
        CHECK_EQ(d.addPct, 100, "T5800.c 满体 → 快照增伤精确 +100%");
    }

    std::cout << "scenario_5800 PASS (1525 满体概率增伤)" << std::endl;
    return 0;
}
