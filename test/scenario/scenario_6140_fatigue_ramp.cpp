// 场景 6140 —— J281 疲惫概率递增族（465，moves_lib/j_fatigue_ramp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6140-6149 → T6140.a-c）
//
// 断言口径：
//   - 审计槽 991465000/100/200 精确断言概率状态机（无随机）：uses 每用 +1、
//     申报概率 = min(base+(uses-1)×step, cap)，命中不清零。
//   - T6140.a 纯链 盾牌猛击 13997 [40,2,10,100]：8 次使用 → 概率序列
//     40/50/60/70/80/90/100/100（PP=10 支撑）。
//   - T6140.b 第 7 次（chance=100）必中：疲惫(码 7) 落地，duration=2 →
//     end_round = 施加回合+2（精确槽位），回合末仍 active。
//   - T6140.c 纯链 烈龙青影斩 15339 [50,1,25,100]：第 3 次 chance=100、duration=1 →
//     施加回合末即到期（end_round = roundCount），回合末不再 active——与 duration=2
//     形成差分，锁"疲惫{1}回合"的时长行参。
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

constexpr int kRampCarrier = 13997;   // 盾牌猛击：465=[40,2,10,100]
constexpr int kOneRoundCarrier = 15339;   // 烈龙青影斩：465=[50,1,25,100]
constexpr int kFiller = 10004;        // 拍打
constexpr int kMaxHp = 99999;
constexpr int kFatigue = 7;

constexpr int kChanceAuditBase = 991465000;
constexpr int kApplyAuditBase  = 991465100;
constexpr int kUsesAuditBase   = 991465200;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kFiller, kFiller, kFiller, kFiller},
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
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6140.a/b 13997：概率递增到 100；第 7 次必中且 duration=2 ══
    {
        ScenarioBattle b(make_party(make_self(kRampCarrier)), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 7; ++r) {
            one_round(b);
            const int expected_chance = std::min(40 + r * 10, 100);
            CHECK_EQ(audit_value(ctx, kChanceAuditBase + 0), expected_chance,
                     "T6140.a 第 " + std::to_string(r + 1) + " 次申报概率");
            CHECK_EQ(audit_value(ctx, kUsesAuditBase + 0), r + 1,
                     "T6140.a 第 " + std::to_string(r + 1) + " 次 uses 计数");
        }
        // 第 7 次（r=6）chance=100 → 必中；duration=2 → end_round = 6+2 = 8。
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kFatigue), 8,
                 "T6140.b 465：100% 档疲惫落地且 duration=2（end=8）");
        CHECK(core_api().has_active_anomaly(&ctx, 1, kFatigue),
              "T6140.b 465：回合末疲惫仍 active");
        // 第 8 次仍 100%（封顶不收窄），并再次刷新（end=9 证明门仍开着）。
        one_round(b);
        CHECK_EQ(audit_value(ctx, kChanceAuditBase + 0), 100,
                 "T6140.a 第 8 次申报概率仍封顶 100");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kFatigue), 9,
                 "T6140.b 465：封顶档再次落地（end=9）");
        std::cout << "  [T6140.a/b] 概率递增+必中 通过 (end="
                  << ctx.get_abnormal_status_end_round(1, kFatigue) << ")\n";
    }

    // ══ T6140.c 15339 duration=1：100% 落地后当回合末即到期 ══
    {
        ScenarioBattle b(make_party(make_self(kOneRoundCarrier)), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 3; ++r) {
            one_round(b);
            CHECK_EQ(audit_value(ctx, kChanceAuditBase + 0), std::min(50 + r * 25, 100),
                     "T6140.c 第 " + std::to_string(r + 1) + " 次申报概率");
        }
        // 第 3 次（r=2）chance=100；duration=1 → end=3，回合末 roundCount=3 → 到期。
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kFatigue), 3,
                 "T6140.c 465：duration=1 → end=3");
        CHECK(!core_api().has_active_anomaly(&ctx, 1, kFatigue),
              "T6140.c 465：duration=1 回合末已到期");
        std::cout << "  [T6140.c] duration=1 差分 通过\n";
    }

    std::cout << "scenario_6140 PASS (J281 疲惫概率递增族)" << std::endl;
    return 0;
}
