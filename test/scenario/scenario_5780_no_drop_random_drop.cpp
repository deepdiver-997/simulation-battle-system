// 场景 5780 —— J247 无降态随机降族（1417，moves_lib/j_no_drop_random_drop.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5780-5789）
//
// 断言口径（精确槽位计数 + 值验证）：
//   - 载体 36079 陷场龙啸 [1417]|[3,1]（特 140，纯链）→ 随机 3 项 -1。
//   - 载体 37617 惊惶缭绕 [1417]|[3,2]（特 140，纯链）→ 随机 3 项 -2（验降幅）。
//   - T5780.a：对手无下降状态 → 六槽中恰 3 槽 == -1、其余 == 0、无正值。
//   - T5780.b：对手防槽已 -1（#3 门闭）→ 不新增下降，只有原 -1 一槽。
//   - T5780.c：37617 → 恰 3 槽 == -2。
//   - 观测面 = 先手 SKILL_EFFECT 结算后（对手尚未行动）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40
constexpr int kDrop1   = 36079;   // [1417]|[3,1] 陷场龙啸：特 140
constexpr int kDrop2   = 37617;   // [1417]|[3,2] 惊惶缭绕：特 140
constexpr int kMaxHp   = 99999;

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 出手并停在先手 SKILL_EFFECT 结算后；pre_defense_drop<0 表示不预置。
void cast_and_stop(int carrier, int pre_defense_drop, BattleContext** out_ctx,
                   ScenarioBattle** out_battle) {
    ScenarioBattle* b = new ScenarioBattle(make_party(mk_self(carrier)), make_party(mk_foe()));
    b->start();
    BattleContext& ctx = b->ctx();
    if (pre_defense_drop != 0) {
        ctx.ability_levels[1][2] = pre_defense_drop;   // 2=防御
    }
    b->submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b->submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b->step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    ctx.debug_step_mode = false;
    *out_ctx = &ctx;
    *out_battle = b;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5780.a 无降态 → 随机 3 项 -1 ══
    {
        BattleContext* ctx = nullptr;
        ScenarioBattle* b = nullptr;
        cast_and_stop(kDrop1, 0, &ctx, &b);
        int neg = 0;
        int zero = 0;
        int pos = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int lv = ctx->ability_levels[1][i];
            if (lv == -1) ++neg;
            else if (lv == 0) ++zero;
            else if (lv > 0) ++pos;
            std::cout << "  [T5780.a] slot " << i << " = " << lv << std::endl;
        }
        CHECK_EQ(neg, 3, "T5780.a 随机 3 项 -1（实测 " + std::to_string(neg) + "）");
        CHECK_EQ(zero, 3, "T5780.a 其余 3 项保持 0");
        CHECK_EQ(pos, 0, "T5780.a 无正值");
        delete b;
    }

    // ══ T5780.b 对手已 -1 → 门闭，不新增 ══
    {
        BattleContext* ctx = nullptr;
        ScenarioBattle* b = nullptr;
        cast_and_stop(kDrop1, -1, &ctx, &b);
        int neg = 0;
        int other = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int lv = ctx->ability_levels[1][i];
            if (lv < 0) ++neg;
            else if (lv != 0) ++other;
        }
        CHECK_EQ(ctx->ability_levels[1][2], -1, "T5780.b 预置防槽 -1 保持");
        CHECK_EQ(neg, 1, "T5780.b 门闭：只有预置的 1 个负槽（实测 " + std::to_string(neg) + "）");
        CHECK_EQ(other, 0, "T5780.b 无其它等级变化");
        delete b;
    }

    // ══ T5780.c 降幅 {1}=2 ══
    {
        BattleContext* ctx = nullptr;
        ScenarioBattle* b = nullptr;
        cast_and_stop(kDrop2, 0, &ctx, &b);
        int neg2 = 0;
        int zero = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int lv = ctx->ability_levels[1][i];
            if (lv == -2) ++neg2;
            else if (lv == 0) ++zero;
            std::cout << "  [T5780.c] slot " << i << " = " << lv << std::endl;
        }
        CHECK_EQ(neg2, 3, "T5780.c 恰 3 槽 -2（实测 " + std::to_string(neg2) + "）");
        CHECK_EQ(zero, 3, "T5780.c 其余 3 槽 0");
        delete b;
    }

    std::cout << "scenario_5780 PASS (1417 无降态随机 N 项降)" << std::endl;
    return 0;
}
