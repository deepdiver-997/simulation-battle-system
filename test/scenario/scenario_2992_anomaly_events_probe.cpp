#include <utils/sbs_rand.h>
// 场景 2992 —— T5a 异常事件探针：EVENT_ANOMALY_CONVERTED / EVENT_ANOMALY_IMMUNED
//（决策清单 T5 剩余缺口，2026-10-21 内核补课；4829 卡布达"每被免疫/转化1次"的前置）
//
// 锁三件事：
//   A. 转化层命中（anomaly_conversion 烧伤→麻痹）且施加落地 → CONVERTED 事件，
//      amount=原始 id、converted_to=转化后 id，目标身上挂的是转化后异常；
//   B. 同种异常覆盖路径（转化后再施加）→ CONVERTED 照发（不吞不重）；
//   C. 免疫票命中（[3] 次免优先于 [6] 转化）→ IMMUNED 事件、CONVERTED 不发
//      ——两事件语义互斥。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/event_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

int g_converted_events = 0;
int g_last_from = -1;
int g_last_to = -1;
int g_immuned_events = 0;
int g_last_immuned_amount = -1;

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

    sbs_srand(2992);
    ScenarioBattle b(make_party(make_pet(0, "施加方", {10001, 10001, 10001, 10001, 10001},
                                         300, 100, 5000)),
                     make_party(make_pet(1, "承伤方", {10001, 10001, 10001, 10001, 10001},
                                         100, 100, 5000)));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.event_center_.register_watcher(
        EventType::EVENT_ANOMALY_CONVERTED, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            ++g_converted_events;
            g_last_from = ev.amount;
            g_last_to = ev.converted_to;
        });
    ctx.event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            ++g_immuned_events;
            g_last_immuned_amount = ev.amount;
        });

    // ── A. 转化层：烧伤(2) → 麻痹(0) ─────────────────────────────────────────
    ctx.anomaly_conversion[1][static_cast<int>(AbnormalStatusId::Burn)] =
        static_cast<int>(AbnormalStatusId::Paralysis);
    ApplyAnomalyResult r = apply_anomaly(&ctx, /*target=*/1,
                                         static_cast<int>(AbnormalStatusId::Burn),
                                         /*duration_rounds=*/3, /*actor=*/0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK(r == ApplyAnomalyResult::CONVERTED,
          "T2992 A：转化施加返回 CONVERTED（实测 " + std::to_string((int)r) + "）");
    CHECK_EQ(g_converted_events, 1, "T2992 A：CONVERTED 事件恰好 1 次");
    CHECK(g_last_from == static_cast<int>(AbnormalStatusId::Burn) &&
              g_last_to == static_cast<int>(AbnormalStatusId::Paralysis),
          "T2992 A：amount=原始烧伤(2)、converted_to=麻痹(0)（实测 from=" +
              std::to_string(g_last_from) + " to=" + std::to_string(g_last_to) + "）");
    CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
          "T2992 A：目标身上挂的是转化后的麻痹");
    CHECK_EQ(g_immuned_events, 0, "T2992 A：无免疫参与，IMMUNED 不发");

    // ── B. 同种覆盖路径：目标已麻痹，再施加（转化）烧伤 → CONVERTED 照发 ──────
    r = apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 5, 0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK(r == ApplyAnomalyResult::CONVERTED || r == ApplyAnomalyResult::DURATION_EXTENDED,
          "T2992 B：覆盖路径返回 CONVERTED/EXTENDED（实测 " + std::to_string((int)r) + "）");
    CHECK_EQ(g_converted_events, 2, "T2992 B：覆盖路径 CONVERTED 照发（累计 2）");

    // ── C. 免疫票优先：次免挡下 → IMMUNED、无 CONVERTED ─────────────────────
    ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/1);
    r = apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 3, 0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK(r == ApplyAnomalyResult::TARGET_IMMUNE,
          "T2992 C：次免命中返回 TARGET_IMMUNE（实测 " + std::to_string((int)r) + "）");
    CHECK_EQ(g_immuned_events, 1, "T2992 C：IMMUNED 事件恰好 1 次");
    CHECK(g_last_immuned_amount == static_cast<int>(AbnormalStatusId::Burn),
          "T2992 C：IMMUNED amount=被挡下的烧伤 id");
    CHECK_EQ(g_converted_events, 2, "T2992 C：免疫优先于转化——CONVERTED 不发");

    // 退化对照：无转化表时施加 → 无 CONVERTED（防事件面误挂）
    idle_round(b);   // 推进一轮清免疫窗，顺带验证事件面在正常对局流中不误发
    ctx.anomaly_conversion[1].clear();
    r = apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Frostbite), 2, 0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    (void)r;
    CHECK_EQ(g_converted_events, 2, "T2992 对照：无转化表 → CONVERTED 不发");

    std::cout << "scenario_2992 PASS (T5a 异常转化/免疫事件探针)" << std::endl;
    return 0;
}
