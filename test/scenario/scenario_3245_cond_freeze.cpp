#include <utils/sbs_rand.h>
// 场景 3245 —— K193 条件冰封族（827，soul_lib/k193_cond_freeze.cpp）
//（2026-10-07，worktree ../agents/k190；号段 3245-3249 → T3245-T3248 四块）
//
// 断言口径：
//   - clause1（80% 冰封）：sbs_srand 钉种子分岔（探针标定）；对手烧伤/冻伤两门向
//     逐 id 断言 Icebound；门闭（无异常）不掷。
//   - clause2（row2 补子句）：024 艾恩斯同款"回合类占位"观测面——给对手挂
//     ContinuousEffect 占位（duration=1），印主属性技命中 → has_round_effects(1)
//     翻false（被断）；row1（无旗）/ 对手带异常 / 攻击技 三向门闭占位存活。
#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <effects/continuousEffect.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：物理攻击载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);
constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);

template <int MarkId>
ElfPet mark_pet(const std::vector<int>& row) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    ElfPet p = make_pet(1, "印主", {kJi, kSuoTou, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "K193 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void act(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 024 同款：给对手挂回合类占位（ROUND_START 桶，duration=1）。
void give_foe_round_marker(BattleContext& ctx) {
    Effect round_effect(90002, 0, /*owner=*/1, /*left_round=*/1,
                        EffectArgs(std::vector<int>{0}));
    auto ce = std::make_unique<ContinuousEffect>(
        round_effect, State::BATTLE_ROUND_START, /*owner=*/1, /*duration=*/1,
        ctx.roundCount);
    ctx.registerEffect(State::BATTLE_ROUND_START, 1, std::move(ce),
                       EffectContainer::Skill);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_1{80, 1};
    const std::vector<int> row_2{80, 1, 1};

    // ══ T3245 clause1 落地：对手烧伤 → 攻击命中冰封（80% 落点 seed）══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
        b.ctx().seed_rng(1);
        act(b, 0);
        CHECK(ctx.has_active_abnormal_status(1, kIcebound),
              "T3245 827：对手烧伤 → 攻击命中冰封（落地 seed）");
        std::cout << "  [T3245a] 烧伤门冰封 通过\n";
    }
    {
        // 冻伤门向 + 失手 seed。
        ScenarioBattle b(make_party(mark_pet<827>(row_1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(try_apply_anomaly(&ctx, 1, kFrostbite, /*duration=*/3, /*actor=*/0));
        b.ctx().seed_rng(2);
        act(b, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kIcebound),
              "T3245 827：失手 seed → 无冰封（冻伤门向）");
        std::cout << "  [T3245b] 冻伤门失手对照 通过\n";
    }
    {
        // 门闭：对手无异常 → 不掷不冰封。
        ScenarioBattle b(make_party(mark_pet<827>(row_1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(1);
        act(b, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kIcebound),
              "T3245 827：门闭（无烧伤/冻伤）→ 不冰封");
        std::cout << "  [T3245c] 门闭对照 通过\n";
    }

    // ══ T3246 clause2（row2）：属性技命中 → 对手回合类被断 ══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        REQUIRE(ctx.has_round_effects(1));
        act(b, 1);   // 缩头（属性技，命中）
        CHECK(!ctx.has_round_effects(1), "T3246 827：属性技命中 → 对手回合类被断");
        std::cout << "  [T3246] 属性技消回合类 通过\n";
    }

    // ══ T3247 clause2 门闭三向：row1 无旗 / 对手带异常 / 攻击技 ══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        act(b, 1);
        CHECK(ctx.has_round_effects(1), "T3247 827：row1 无补子句旗 → 不断");
        std::cout << "  [T3247a] row1 无旗对照 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
        give_foe_round_marker(ctx);
        act(b, 1);
        CHECK(ctx.has_round_effects(1), "T3247 827：对手带异常 → 补子句门闭不断");
        std::cout << "  [T3247b] 异常门闭对照 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        act(b, 0);   // 撞击（攻击技）
        CHECK(ctx.has_round_effects(1), "T3247 827：攻击技 → 属性技门闭不断");
        std::cout << "  [T3247c] 攻击技门闭对照 通过\n";
    }

    // ══ T3248 进化行互锚：row2 同参 [80,...] 冰封照常（切片 2/3 行参透传）══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
        b.ctx().seed_rng(1);
        act(b, 0);
        CHECK(ctx.has_active_abnormal_status(1, kIcebound),
              "T3248 827：row2 冰封支同参生效（互锚）");
        std::cout << "  [T3248] row2 互锚 通过\n";
    }

    std::cout << "scenario_3245 PASS (K193 条件冰封族)" << std::endl;
    return 0;
}
