// 场景 6120 —— J279 异常施加刷新族（1586，moves_lib/j_anomaly_apply_refresh.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6120-6129 → T6120.a-c）
//
// 断言口径：
//   - 载体 蔑川燎原 36149（纯链 [1586]|[100,2,2,5]）= 100% 令对手烧伤(2)；
//     若对手已处于烧伤(2) → 刷新至 5 回合。
//   - T6120.a 施加分支：对手无异常 → 烧伤落地（精确码 2）、end_round = 当前回合+2；
//     刷新审计 0、施加审计 ≠ -1。
//   - T6120.b 刷新分支：预置烧伤 end=2 → 不重复施加，end_round 被刷新到 5；
//     刷新审计 = 1、施加审计保持 -1。
//   - T6120.c 检查码精确：预置中毒(1) → 不触发刷新（{2}=烧伤），改走施加分支
//     追加烧伤(2)；中毒剩余回合不受影响（槽位级比对，非"异常数量和"）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 36149;   // 蔑川燎原：1586=[100,2,2,5]
constexpr int kFiller  = 10004;   // 拍打
constexpr int kMaxHp   = 99999;

constexpr int kBurn   = 2;
constexpr int kPoison = 1;

constexpr int kRefreshAuditBase = 991586000;
constexpr int kApplyAuditBase   = 991586100;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe() {
    ElfPet p = make_pet(2, "对手", {kFiller, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int remaining(BattleContext& ctx, int side, int code) {
    return ctx.get_abnormal_status_end_round(side, code) - ctx.roundCount;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6120.a 施加分支：无异常 → 100% 烧伤落地（持续 2）══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);
        CHECK(core_api().has_active_anomaly(&ctx, 1, kBurn), "T6120.a 1586：烧伤(2) 已落地");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kBurn), 2,
                 "T6120.a 1586：施加 end_round = 当前回合+2（持续 2）");
        CHECK_EQ(audit_value(ctx, kRefreshAuditBase + 0), 0,
                 "T6120.a 1586：未走刷新分支");
        CHECK(audit_value(ctx, kApplyAuditBase + 0) != -1,
              "T6120.a 1586：施加分支已执行（审计写入结果码）");
        CHECK(!core_api().has_active_anomaly(&ctx, 1, 5), "T6120.a 1586：未误挂冻伤(5)");
        std::cout << "  [T6120.a] 施加分支 通过 (res=" << audit_value(ctx, kApplyAuditBase)
                  << ")\n";
    }

    // ══ T6120.b 刷新分支：预置烧伤 2 回合 → 刷新至 5 回合，不重复施加 ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)core_api().apply_anomaly(&ctx, 1, kBurn, 2, 0);
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kBurn), 2, "T6120.b 前置：预置烧伤 end=2");
        one_round(b);
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kBurn), 5,
                 "T6120.b 1586：刷新至 end_round=5（5 回合）");
        CHECK_EQ(audit_value(ctx, kRefreshAuditBase + 0), 1,
                 "T6120.b 1586：刷新分支命中 1 次");
        CHECK_EQ(audit_value(ctx, kApplyAuditBase + 0), -1,
                 "T6120.b 1586：刷新分支不重复施加");
        std::cout << "  [T6120.b] 刷新分支 通过 (rem=" << remaining(ctx, 1, kBurn) << ")\n";
    }

    // ══ T6120.c 检查码精确：对手中毒(1)、无烧伤 → 不刷新，追加烧伤(2) ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)core_api().apply_anomaly(&ctx, 1, kPoison, 2, 0);
        const int poison_end_before = ctx.get_abnormal_status_end_round(1, kPoison);
        CHECK_EQ(poison_end_before, 2, "T6120.c 前置：预置中毒 end=2");
        one_round(b);
        CHECK_EQ(audit_value(ctx, kRefreshAuditBase + 0), 0,
                 "T6120.c 1586：中毒不触发烧伤刷新门");
        CHECK(core_api().has_active_anomaly(&ctx, 1, kBurn),
              "T6120.c 1586：改走施加分支 → 烧伤落地");
        CHECK(core_api().has_active_anomaly(&ctx, 1, kPoison),
              "T6120.c 1586：原有中毒未被顶替");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kPoison), poison_end_before,
                 "T6120.c 1586：中毒 end_round 不变（槽位级）");
        std::cout << "  [T6120.c] 检查码精确 通过\n";
    }

    std::cout << "scenario_6120 PASS (J279 异常施加刷新族)" << std::endl;
    return 0;
}
