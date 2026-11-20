// 场景 4650 —— K309 回合类消除族（802，soul_lib/k309_round_clear.cpp）
//（2026-10-21，worktree ../agents/k309；号段 4650-4659 → T4650-T4652 三块）
//
// 断言口径（802「回合开始时，若对手不处于异常状态，则当回合自身攻击技能消除对手
// 回合类效果」）：
//   - T4650 **消除生效**：给对手挂 1 回合效果 → 印主用攻击技出手 → 对手的回合类
//     效果被消除（has_round_effects(1) 由 true 变 false）。
//   - T4651 **门闭（对手有异常）**：对手处于**异常状态**（本单门是"对手**不**处于
//     异常状态"）→ 印主用攻击技出手 → 对手回合类效果**不被**消除。
//   - T4652 **属性技门闭**：印主用**属性技**出手 → 不消除（官方"自身**攻击**技能"）。
//
// ⚠️ 场景构造口径：零参行 → args 传 {0,0}（DB 原样；实现不消费任何下标）。
// ⚠️ **时序**：本单的条件在 **ROUND_START** 判（"回合开始时…"），消除在
//   **SKILL_EFFECT**。故场景流程 = ①start() ②给对手注册 1 回合效果 ③**推进一回合**
//   （使其经历一次 ROUND_START → 条件被判定）④再注册一个新的 1 回合效果（供本回合
//   消除断言用）⑤印主出手 → 断言。
//   ⚠️ 对手注册的效果挂在 ROUND_START 桶、owner=对手；推进一回合后它会自然到期，
//   故每轮都要**重新注册**一个供断言用。
// ⚠️ 造"对手处于异常状态"用 apply_anomaly 直铺（K255 320 场景同款）。
#include <cstdlib>
#include <iostream>
#include <memory>
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

constexpr int kPaDa = 10004;   // 拍打：物理攻击技
constexpr int kSuoTou = 20004;  // 瞪眼：属性技
constexpr int kMaxHp = 99999;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

SoulMark make_mark() {
    // 零参行：DB args 原样 {0,0}，实现不消费任何下标。
    SoulMark sm(802, "K309 魂印", "test", EffectArgs(std::vector<int>{0, 0}));
    sm.monster_id = 3157;
    return sm;
}

ElfPet mark_pet(int skill0 = kPaDa) {
    ElfPet p = make_pet(1, "冥王·裂空破", {skill0, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/100, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 给对手注册 1 回合效果（无逻辑 no-op，仅占回合效果位）——scenario_009 同款。
void register_foe_round_effect(ScenarioBattle& b) {
    Effect round_effect(90001, 0, /*owner=*/1, /*left_round=*/1,
                        EffectArgs(std::vector<int>{0, 1}));
    auto ce = std::make_unique<ContinuousEffect>(
        round_effect, State::BATTLE_ROUND_START, /*owner=*/1, /*duration=*/1,
        b.ctx().roundCount);
    b.ctx().registerEffect(State::BATTLE_ROUND_START, 1, std::move(ce),
                           EffectContainer::Skill);
}

// 推进一回合（双方 NONE）——让 ROUND_START 条件判定跑一遍。
void advance_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4650 消除生效：对手有回合类效果 + 印主攻击技出手 → 被消除 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        register_foe_round_effect(b);   // 对手挂 1 回合效果
        advance_round(b);                // 推进一回合：条件在 ROUND_START 判定（对手无异常）
        register_foe_round_effect(b);   // 再挂一个供本回合断言用
        REQUIRE(ctx.has_round_effects(1));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(!ctx.has_round_effects(1),
              "T4650 802：对手不处于异常状态 + 印主用攻击技能 → 消除对手回合类效果");
        std::cout << "  [T4650] 消除生效 通过\n";
    }

    // ══ T4651 门闭：对手处于异常状态 → 不消除 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
        register_foe_round_effect(b);
        advance_round(b);                // ROUND_START：对手有异常 → 条件不成立
        register_foe_round_effect(b);
        REQUIRE(ctx.has_round_effects(1));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.has_round_effects(1),
              "T4651 802：对手处于异常状态 → 门闭，不消除对手回合类效果");
        std::cout << "  [T4651] 异常门闭对照 通过\n";
    }

    // ══ T4652 属性技门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*skill0=*/kSuoTou)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        register_foe_round_effect(b);
        advance_round(b);
        register_foe_round_effect(b);
        REQUIRE(ctx.has_round_effects(1));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.has_round_effects(1),
              "T4652 802：用属性技 → 不消除（官方'自身攻击技能'），对手回合类效果仍在");
        std::cout << "  [T4652] 属性技门闭对照 通过\n";
    }

    std::cout << "scenario_4650 PASS (K309 回合类消除族)" << std::endl;
    return 0;
}
