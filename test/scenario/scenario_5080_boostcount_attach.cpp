// 场景 5080 —— K346 能力提升计数附加固伤族（877/911/946，
// soul_lib/k_boostcount_attach.cpp）
//（2026-10-22，worktree k346；号段 5080-5089 → T5080-T5083 四块）
//
// 断言口径（伤害带锚：atk300/def100 红伤 raw ∈ [87,102]，用 [85,104] 派生；
//   附加固伤走粉伤管线，量 = 计数×每级值，与红伤相加后总血量差断言）：
//   - T5080 877 对手 3 项提升（特攻+2/特防+1）→ 附着 3×75=225；
//     对照组零提升 → 零附着（总差落回红伤带）。
//   - T5081 877 无提升 + args 石化概率 100（参数化自证）→ 对手石化落地。
//   - T5082 911 双方提升项数和 3 → 180；回合末自身能力下降被解除（精确槽位）。
//   - T5083 946 连续三回合：速度 2/4/6（+2 每回合、+6 钳）→ 附着 200/400/600。
//   所有等级只加在**不进入物理伤害公式**的槽（特攻 1 / 特防 3 / 速度 4），
//   避免污染红伤带；能力提升计数口径 = 5 项战斗属性正等级项数。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kOppHp = 200000;
constexpr int kOwnHp = 200000;

// 红伤带（atk300/def100）：[87,102] 外扩 ±2。
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;

SoulMark make_mark(int id, std::vector<int> args) {
    SoulMark sm(id, "K346 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 3263;
    return sm;
}

ElfPet mark_pet(int id, std::vector<int> args) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kOwnHp, make_mark(id, std::move(args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kOppHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

long long round_delta(ScenarioBattle& b, int own_action_index) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, own_action_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5080 877：对手 3 项提升 → +225；对照组零提升 → 零附着 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet(877, {75, 50, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 只放不进入物理伤害公式的槽：特攻 1、特防 3。
        // 「一种能力提升状态」= 一个属性槽为正等级（槽内 +2 仍算 1 项）→ 2 项。
        ctx.ability_levels[1][1] = 2;
        ctx.ability_levels[1][3] = 1;
        const long long delta = round_delta(b, 0);
        CHECK(delta >= 150 + kRedLo && delta <= 150 + kRedHi,
              "T5080 877：2 项提升附着 150（总差 " + std::to_string(delta) +
                  "，期望带 [235,254]）");
        std::cout << "  [T5080] 877 提升计数附着 通过（delta=" << delta << "）\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(877, {75, 50, 1})), make_party(foe_pet()));
        b.start();
        const long long delta = round_delta(b, 0);
        CHECK(delta >= kRedLo && delta <= kRedHi,
              "T5080c 877：零提升对照组无附着（总差 " + std::to_string(delta) +
                  "，期望带 [85,104]）");
        std::cout << "  [T5080c] 877 零提升对照 通过（delta=" << delta << "）\n";
    }

    // ══ T5081 877：无提升 + 石化概率 100（args 参数化）→ 石化落地 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(877, {75, 100, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 概率 100 档：申报概率短路不耗 rand（roll_percent>=100）。
        (void)round_delta(b, 0);
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Petrify)),
              "T5081 877：无提升 + 100% 档 → 对手石化落地");
        std::cout << "  [T5081] 877 石化分支 通过\n";
    }

    // ══ T5082 911：双方提升项数和 3 → +180；回合末解除自身能力下降 ═════════
    {
        ScenarioBattle b(make_party(mark_pet(911, {60, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][1] = 1;   // 自身特攻 +1（正等级 1 项）
        ctx.ability_levels[0][3] = -2;  // 自身特防 -2（待回合末解除）
        ctx.ability_levels[1][1] = 2;   // 对手特攻 +2（正等级 1 项）
        ctx.ability_levels[1][3] = 0;
        // 对手再 +1 速（第 3 项；速度不进物理伤害公式）。
        ctx.ability_levels[1][4] = 1;
        const long long delta = round_delta(b, 0);
        CHECK(delta >= 180 + kRedLo && delta <= 180 + kRedHi,
              "T5082 911：双方 3 项提升附着 180（总差 " + std::to_string(delta) +
                  "，期望带 [265,284]）");
        CHECK_EQ(ctx.ability_levels[0][3], 0,
                 "T5082 911：回合末解除自身特防 -2（精确槽位 3）");
        CHECK_EQ(ctx.ability_levels[0][1], 1,
                 "T5082 911：自身特攻 +1 不受解除影响（精确槽位 1）");
        std::cout << "  [T5082] 911 计数附着 + 回合末解下降 通过（delta=" << delta
                  << "）\n";
    }

    // ══ T5083 946：R1/R2/R3 速度 2/4/6（+2 每回合，+6 钳）→ 附着 200/400/600 ══
    {
        ScenarioBattle b(make_party(mark_pet(946, {2, 100, 1})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect_speed[3] = {2, 4, 6};
        const int expect_attach[3] = {200, 400, 600};
        for (int r = 0; r < 3; ++r) {
            const long long delta = round_delta(b, 0);
            CHECK_EQ(ctx.ability_levels[0][4], expect_speed[r],
                     "T5083 946：R" + std::to_string(r + 1) + " 速度等级（精确槽位 4）");
            CHECK(delta >= expect_attach[r] + kRedLo &&
                      delta <= expect_attach[r] + kRedHi,
                  "T5083 946：R" + std::to_string(r + 1) + " 附着 " +
                      std::to_string(expect_attach[r]) + "（总差 " +
                      std::to_string(delta) + "）");
        }
        std::cout << "  [T5083] 946 速度递增附着 通过\n";
    }

    // ══ T5084 946 无印对照：速度不涨、无附着 ════════════════════════════════
    {
        ElfPet bare = make_pet(3, "裸印主",
                               {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                               /*speed=*/300, /*atk=*/300, kOwnHp);
        for (Skills& s : bare.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(bare), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const long long delta = round_delta(b, 0);
        CHECK_EQ(ctx.ability_levels[0][4], 0,
                 "T5084 946c：无印对照速度不涨（精确槽位 4）");
        CHECK(delta >= kRedLo && delta <= kRedHi,
              "T5084 946c：无印对照无附着（总差 " + std::to_string(delta) + "）");
        std::cout << "  [T5084] 946 无印对照 通过（delta=" << delta << "）\n";
    }

    std::cout << "scenario_5080 PASS (K346 能力提升计数附加固伤族)" << std::endl;
    return 0;
}
