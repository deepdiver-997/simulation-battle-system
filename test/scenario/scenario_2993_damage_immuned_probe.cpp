#include <utils/sbs_rand.h>
// 场景 2993 —— T5b 伤害类免疫事件探针：EVENT_DAMAGE_IMMUNED
//（决策清单 T5 剩余缺口，2026-10-21 内核补课；4830 鲨鱼辣椒"对手免疫固定伤害
//  则消除回合类"的检测点）
//
// 锁四件事：
//   A. 粉伤免疫票（counts=1）挡下 → 事件 amount=挡前值、kind=FIXED、actor/target 归因，
//      本体 hp 零变化；
//   B. 次数型只挡一段——第二段粉伤照常落地、不再发事件；
//   C. 红伤免疫票（DAMAGE，counts=1）挡下 → kind=NORMAL、hp 零变化；
//   D. 与 EVENT_ATTACK_BLOCKED 的分工：票挡下才发本事件（本探针不直接断言 BLOCKED，
//      只锁"无票时粉伤落地且 DAMAGE_IMMUNED 不发"的对照）。
#include <iostream>

#include <effects/event_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

int g_events = 0;
int g_last_amount = -1;
int g_last_kind = -1;
int g_last_actor = -1;
int g_last_target = -1;

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(2993);
    ScenarioBattle b(make_party(make_pet(0, "施加方", {10001, 10001, 10001, 10001, 10001},
                                         300, 100, 5000)),
                     make_party(make_pet(1, "承伤方", {10001, 10001, 10001, 10001, 10001},
                                         100, 100, 5000)));
    b.start();
    BattleContext& ctx = b.ctx();
    for (Skills& s : ctx.getPet(0).skills) {
        s.must_hit = 1;    // 攻击半边（C 组）用真实技能结算，保命中排除 miss 噪声
        s.accuracy = 100;
    }
    ctx.event_center_.register_watcher(
        EventType::EVENT_DAMAGE_IMMUNED, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            ++g_events;
            g_last_amount = ev.amount;
            g_last_kind = ev.damage_kind;
            g_last_actor = ev.actor;
            g_last_target = ev.target;
        });

    // ── D 对照先行：无票时粉伤照常落地、事件不发 ─────────────────────────────
    const int hp0 = ctx.getPet(1).hp;
    deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, /*actor=*/0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK_EQ(ctx.getPet(1).hp, hp0 - 100, "T2993 D：无票粉伤照常落地");
    CHECK_EQ(g_events, 0, "T2993 D：无票不发 DAMAGE_IMMUNED");

    // ── A. 粉伤免疫票（counts=1）挡下 ────────────────────────────────────────
    ctx.grant_immunity(/*owner=*/1, ImmunityType::PINK_DAMAGE, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/1);
    deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, /*actor=*/0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK_EQ(ctx.getPet(1).hp, hp0 - 100, "T2993 A：票挡下——本体 hp 零变化");
    CHECK_EQ(g_events, 1, "T2993 A：DAMAGE_IMMUNED 恰好 1 次");
    CHECK(g_last_amount == 100 && g_last_kind == static_cast<int>(DamageKind::FIXED) &&
              g_last_actor == 0 && g_last_target == 1,
          "T2993 A：amount=100/kind=FIXED/actor=0/target=1（实测 amount=" +
              std::to_string(g_last_amount) + " kind=" + std::to_string(g_last_kind) +
              " actor=" + std::to_string(g_last_actor) + " target=" +
              std::to_string(g_last_target) + "）");

    // ── B. 次数型只挡一段：第二段照常落地 ───────────────────────────────────
    deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, /*actor=*/0);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK_EQ(ctx.getPet(1).hp, hp0 - 200, "T2993 B：次免耗尽——第二段落地");
    CHECK_EQ(g_events, 1, "T2993 B：第二段不再发事件");

    // ── C. 红伤免疫票（DAMAGE，counts=1）挡下 ────────────────────────────────
    //（⚠️ deal_damage(NORMAL) 直调是裸扣血路径、不跑相管线——BLOCK 相只在技能攻击
    //  结算里跑，故本组用真实攻击结算驱动：攻方用撞击，票在挡下后应发事件且零掉血。）
    ctx.grant_immunity(/*owner=*/1, ImmunityType::DAMAGE, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/1);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 撞击
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    CHECK_EQ(ctx.getPet(1).hp, hp0 - 200, "T2993 C：红伤票挡下——hp 零变化");
    CHECK_EQ(g_events, 2, "T2993 C：红伤侧事件 1 次");
    CHECK(g_last_kind == static_cast<int>(DamageKind::NORMAL) && g_last_actor == 0 &&
              g_last_target == 1,
          "T2993 C：kind=NORMAL/actor=0/target=1（实测 kind=" +
              std::to_string(g_last_kind) + " amount=" + std::to_string(g_last_amount) +
              "）");

    std::cout << "scenario_2993 PASS (T5b 伤害类免疫事件探针)" << std::endl;
    return 0;
}
