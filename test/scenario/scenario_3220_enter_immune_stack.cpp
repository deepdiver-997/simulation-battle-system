// 场景 3220 —— K190 出场低血免疫弹控窗+受击叠防族（870，soul_lib/k190_enter_immune_stack.cpp）
//（2026-10-07，worktree ../agents/k190；号段 3220-3229 → T3220-T3223 四块）
//
// 断言口径：
//   - 免疫弹控窗（191 配对）：低血出场 → try_apply_anomaly 打我返回 false（免疫档）、
//     异常落到施加方（REFLECTED）；高血出场门闭 → 异常正常落地。免疫票 duration=db[0]
//     引擎到期 → 窗后落地（回弹票随 RuleCenter 生命周期，窗后行为本单不断言——记档）。
//   - 受击叠防：ability_levels 槽 2/3 逐回合精确；低血翻倍 +2、非低血 +1（每次受击）。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;   // 撞击
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

// hp 低/高 = 出场门的两档；row = 官方行参。
template <int MarkId>
ElfPet mark_pet(const std::vector<int>& row, int hp) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K190 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_2{2, 1, 1};
    const std::vector<int> row_3{3, 1, 1};

    // ══ T3220 低血出场 → 免疫并反弹（2 回合窗内）══
    {
        ScenarioBattle b(make_party(mark_pet<870>(row_2, /*hp=*/100)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const bool landed = try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1);
        CHECK(!landed, "T3220 870：窗内施加 → 未落地（免疫档）");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T3220 870：我无烧伤");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T3220 870：反弹 → 施加方烧伤");
        std::cout << "  [T3220] 低血免疫弹控窗 通过\n";
    }

    // ══ T3221 高血出场门闭 → 异常正常落地；窗过期后免疫失效（2 回合后）══
    {
        ScenarioBattle b(make_party(mark_pet<870>(row_2, /*hp=*/99999)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T3221 870：高血出场 → 门闭，异常正常落地");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn),
              "T3221 870：门闭无反弹");
        std::cout << "  [T3221a] 高血门闭 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<870>(row_2, /*hp=*/100)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        idle_round(b);   // R1
        idle_round(b);   // R2（窗长 2 到期）
        CHECK(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T3221 870：2 回合窗过期 → 免疫失效，异常落地");
        std::cout << "  [T3221b] 窗过期失效 通过\n";
    }

    // ══ T3222 受击叠防：低血翻倍 +2/次；非低血 +1/次 ══
    {
        ScenarioBattle b(make_party(mark_pet<870>(row_2, /*hp=*/100)),
                         make_party(plain_pet(/*atk=*/50)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][2], 2, "T3222 870：低血受击 → 防 +2（翻倍）");
        CHECK_EQ(ctx.ability_levels[0][3], 2, "T3222 870：低血受击 → 特防 +2（翻倍）");
        std::cout << "  [T3222a] 低血叠防翻倍 通过\n";
    }
    {
        // 非低血档：印主满血 99999 vs 对手 50000——受击后仍高于对手（判定取受击时点现值，
        // 同满血对局挨打后必然低于对手 → 用真高体构造）。
        ScenarioBattle b(make_party(mark_pet<870>(row_2, /*hp=*/99999)),
                         make_party(plain_pet(/*atk=*/50, /*hp=*/50000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][2], 1, "T3222 870：非低血受击 → 防 +1");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "T3222 870：非低血受击 → 特防 +1");
        std::cout << "  [T3222b] 非低血叠防 通过\n";
    }

    // ══ T3223 进化行 [3 回合窗]：窗内反弹互锚（db[0] 透传）══
    {
        ScenarioBattle b(make_party(mark_pet<870>(row_3, /*hp=*/100)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const bool landed = try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1);
        CHECK(!landed && ctx.has_active_abnormal_status(1, kBurn),
              "T3223 870 进化行：3 回合窗内免疫+反弹（db[0]=3 透传）");
        std::cout << "  [T3223] 进化行互锚 通过\n";
    }

    std::cout << "scenario_3220 PASS (K190 出场低血免疫弹控窗+受击叠防族)" << std::endl;
    return 0;
}
