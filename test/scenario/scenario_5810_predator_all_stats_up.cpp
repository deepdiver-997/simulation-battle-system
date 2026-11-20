// 场景 5810 —— J250 天敌全属性强化族（2057，moves_lib/j_predator_all_stats_up.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5810-5819）
//
// 断言口径（精确六槽快照 + 天敌关系直证）：
//   - 载体 29290 活化姿态 [2057,843]|[1,2,2]（属性技；843 先制，不碰等级）。
//   - T5810.a：双方类型 {0,0}（无克制）→ 全属性 +1（六槽逐槽快照）。
//   - T5810.b：用 core_api().restraint_multiplier 暴力选一对"对手克制自身"的固有条
//     属性（is_predator 口径）→ 全属性 +2（强化效果翻倍）。
//   - 观测面 = 先手 SKILL_EFFECT 结算后（属性技无伤害阶段；对手尚未行动）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40
constexpr int kAllUp   = 29290;   // [2057,843]|[1,2,2] 活化姿态：属性技
constexpr int kMaxHp   = 99999;

ElfPet mk_self(std::array<int, 2> types) {
    ElfPet p = make_pet(1, "己方", {kAllUp, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    p.elementalAttributes = types;
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe(std::array<int, 2> types) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    p.elementalAttributes = types;
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

void cast_and_stop(std::array<int, 2> self_types, std::array<int, 2> foe_types,
                   BattleContext** out_ctx, ScenarioBattle** out_battle) {
    ScenarioBattle* b = new ScenarioBattle(make_party(mk_self(self_types)),
                                           make_party(mk_foe(foe_types)));
    b->start();
    BattleContext& ctx = b->ctx();
    b->submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b->submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b->step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    ctx.debug_step_mode = false;
    *out_ctx = &ctx;
    *out_battle = b;
}

void check_all_slots(BattleContext& ctx, int expected, const std::string& label) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(ctx.ability_levels[0][i], expected,
                 label + " 自身槽 " + std::to_string(i) + " 等级");
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5810.a 无克制 → 全属性 +1 ══
    {
        BattleContext* ctx = nullptr;
        ScenarioBattle* b = nullptr;
        cast_and_stop({0, 0}, {0, 0}, &ctx, &b);
        check_all_slots(*ctx, 1, "T5810.a 无克制");
        std::cout << "  [T5810.a] neutral types -> all stats +1 通过\n";
        delete b;
    }

    // ══ T5810.b 天敌关系 → 全属性 +2 ══
    {
        int self_type = -1;
        int foe_type = -1;
        for (int a = 0; a <= 30 && self_type < 0; ++a) {
            for (int d = 0; d <= 30; ++d) {
                const int self_pair[2] = {a, 0};
                const int foe_pair[2] = {d, 0};
                if (core_api().restraint_multiplier(foe_pair, self_pair) > 1.0) {
                    self_type = a;
                    foe_type = d;
                    break;
                }
            }
        }
        CHECK(self_type >= 0 && foe_type >= 0, "T5810.b 找到一对天敌克制组合");
        std::cout << "  [T5810.b] predator pair self=" << self_type
                  << " foe=" << foe_type << std::endl;
        BattleContext* ctx = nullptr;
        ScenarioBattle* b = nullptr;
        cast_and_stop({self_type, 0}, {foe_type, 0}, &ctx, &b);
        check_all_slots(*ctx, 2, "T5810.b 天敌");
        std::cout << "  [T5810.b] predator -> all stats +2 通过\n";
        delete b;
    }

    std::cout << "scenario_5810 PASS (2057 天敌全属性强化翻倍)" << std::endl;
    return 0;
}
