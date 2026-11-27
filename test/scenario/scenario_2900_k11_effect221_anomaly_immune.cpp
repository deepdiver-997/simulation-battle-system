#include <utils/sbs_rand.h>
// 场景 2900 —— K11 effect 221：出战首回合免疫所有异常
//
// 断言口径（异常票走 grant_immunity 内核面，is_immune 在 apply_anomaly 入口消费）：
//   A round1：对持有方施加烧伤 → 被免疫（has_active false）。
//   B round2：票已到期 → 施加成功。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder(int id, int monster_id) {
    return scenario::make_pet(id, "221持印者", {10005, 10005, 10005, 10005, 10005},
                              200 + id, 10, 100000,
                              make_mark(221, {}, monster_id));
}

ElfPet plain(int id) {
    return scenario::make_pet(id, "无印替补", {10005, 10005, 10005, 10005, 10005},
                              100, 10, 100000);
}

int sole_221_handle(const BattleContext& ctx, int owner) {
    int count = 0;
    int handle = 0;
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category != RuleCategory::IMMUNE || t.source_effect_id != 221
            || t.source_owner != owner) {
            continue;
        }
        ++count;
        handle = t.source_id;
        CHECK_EQ(t.target, owner, "221：票必须保护各自持印方");
        CHECK_EQ(t.subtype, static_cast<int>(ImmunityType::ANOMALY), "221：异常免疫票");
        CHECK_EQ(t.anomaly_mask, 0, "221：全异常而非单种异常");
        CHECK_EQ(t.remaining_rounds, 1, "221：仍沿用一回合绝对窗口");
        CHECK_EQ(t.remaining_counts, 0, "221：窗口票不因多次挡异常消耗");
        CHECK_EQ(t.register_round, ctx.roundCount, "221：在本次真实登场回合起算");
        CHECK(t.soul && t.scope == EffectScope::ON_STAGE, "221：魂免且离场清票");
    }
    CHECK_EQ(count, 1, "221：每方恰一张票，early+登场桶不叠票");
    CHECK(handle > 0, "221：由 RuleCenter 分配正值唯一句柄");
    return handle;
}

void idle_round(ScenarioBattle& b) {
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

    ScenarioBattle b(
        scenario::make_party({scenario::make_pet(1, "持有方",
                                                 {10005, 10005, 10005, 10005, 10005}, 100, 10,
                                                 1000000, make_mark(221, {}, 2476))}),
        scenario::make_party(scenario::make_pet(2, "对手", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 1000000)));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();

    // A round1：免疫
    apply_anomaly(&ctx, /*target=*/0, /*烧伤=*/2, /*duration_rounds=*/3, /*actor=*/1);
    CHECK(!ctx.has_active_abnormal_status(0, 2), "A 首回合全异常免疫（烧伤被拒）");

    // B round2：票到期
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    apply_anomaly(&ctx, 0, 2, 3, 1);
    CHECK(ctx.has_active_abnormal_status(0, 2), "B 次回合票到期 → 施加成功");

    // 官方两行 args 均空：[2476] / [1864,1865]。双方同时持印不得共用正值句柄。
    for (const int monster_id : {1864, 1865}) {
        auto party0 = scenario::make_party(plain(10));
        auto party1 = scenario::make_party(plain(20));
        party0[0] = holder(1, 2476);
        party1[0] = holder(2, monster_id);
        ScenarioBattle duel(party0, party1);
        BattleContext& dctx = duel.ctx();
        duel.start();   // 真实 ENTER handler 已执行 early 和双方登场桶
        const auto& program = dctx.getPet(0).soulMark.program_;
        REQUIRE(program.size() == 1);
        CHECK(program[0].early, "221：保留开场/主动登场即生效的 early 节点");
        CHECK(program[0].trigger_state == State::OPERATION_ENTER_EXIT_STAGE,
              "221：只收敛实现，不改登场时点");
        const int handle0 = sole_221_handle(dctx, 0);
        const int handle1 = sole_221_handle(dctx, 1);
        CHECK(handle0 != handle1, "221：双方句柄独立，不能先按固定句柄跨 owner 刷新");
        for (int side = 0; side < 2; ++side) {
            CHECK(!try_apply_anomaly(&dctx, side, 2, 3, 1 - side),
                  "221：双方首回合均免烧伤");
            CHECK(!try_apply_anomaly(&dctx, side, 0, 3, 1 - side),
                  "221：同一窗口多次挡不同异常，不消耗成次数票");
        }
        // 同一回合再执行真实已注册桶，覆盖键必须复用各自句柄且不增加票数。
        dctx.execute_registered_actions(0, State::OPERATION_ENTER_EXIT_STAGE);
        dctx.execute_registered_actions(1, State::OPERATION_ENTER_EXIT_STAGE);
        CHECK_EQ(sole_221_handle(dctx, 0), handle0, "221：owner0 early+桶幂等");
        CHECK_EQ(sole_221_handle(dctx, 1), handle1, "221：owner1 early+桶幂等");
        idle_round(duel);
        for (int side = 0; side < 2; ++side) {
            CHECK(try_apply_anomaly(&dctx, side, 2, 3, 1 - side),
                  "221：双方均只保护登场一回合，次回合烧伤可落地");
        }
    }

    // 主动切宠走 harness 的 CHOOSE_PET，不手动挂票或改 on_stage 假造登场。
    {
        auto party0 = scenario::make_party(plain(10));
        auto party1 = scenario::make_party(plain(20));
        party0[0] = holder(1, 2476);
        party1[0] = holder(2, 1864);
        ScenarioBattle duel(party0, party1);
        BattleContext& dctx = duel.ctx();
        duel.start();
        const int other_handle = sole_221_handle(dctx, 1);
        duel.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        REQUIRE(dctx.on_stage[0] == 1);
        CHECK(try_apply_anomaly(&dctx, 0, 2, 3, 1), "221：离场票不泄漏给无印替补");
        CHECK_EQ(sole_221_handle(dctx, 1), other_handle, "221：我方切宠不拆对手的票");
        CHECK(!try_apply_anomaly(&dctx, 1, 2, 3, 0), "221：对手仍处于自己的首回合窗口");
        duel.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        duel.drive();

        duel.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        REQUIRE(dctx.on_stage[0] == 0);
        sole_221_handle(dctx, 0);   // early 在操作收到时已授予，不等历史开场桶再次执行
        CHECK(!try_apply_anomaly(&dctx, 0, 2, 3, 1), "221：主动重新登场立即重新获得窗口");
        duel.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        duel.drive();
        CHECK(try_apply_anomaly(&dctx, 0, 2, 3, 1), "221：再登场仍只免当次首回合，不延长");
    }

    std::cout << "scenario_2900 PASS (K11 effect221 首回合全异常免疫)" << std::endl;
    return 0;
}
