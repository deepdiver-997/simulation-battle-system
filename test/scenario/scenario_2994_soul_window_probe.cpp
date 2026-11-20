#include <utils/sbs_rand.h>
// 场景 2994 —— T6 魂印桶窗界一等公民探针（决策清单 K3/T6，2026-10-21 落地）
//
// 病灶（j-prod-3 实测）：soulmark 更新器每回合重注册 → 节点 duration 被无限复位，
// "持续3回合"的窗 R4 仍触发——实现只能手写 plugin_storage 账本绕（K7/K12 模式）。
//
// 本探针锁三件事：
//   A. duration_rounds=3 的节点自首次注册起**恰好执行 3 回合**（R1-R3），R4/R5 静默；
//   B. 离场（deactivate 清锚）后再由更新器重注册 → **重开窗**再执行 3 回合
//     （"登场获得，持续N回合"族的再上场语义）；
//   C. duration_rounds=0 的兄弟节点不受影响（永久，每回合照跑——既有语义零漂移）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/soul_mark_manager.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMarkWindow = 99771;   // 窗节点 + 永久对照节点

int g_window_fires = 0;
int g_forever_fires = 0;

EffectResult window_node(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    ++g_window_fires;
    return EffectResult::kOk;
}

EffectResult forever_node(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    ++g_forever_fires;
    return EffectResult::kOk;
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

    // 注册测试程序（构造 SoulMark 之前——ctor 从 manager 取程序）：
    // 节点0 = 3 回合窗；节点1 = 永久对照（duration_rounds 缺省 0）。
    SoulMarkManager::getInstance().registerSoulMarkProgram(
        kMarkWindow,
        {SoulMarkNodeRef(State::BATTLE_ROUND_START, &window_node,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE,
                         /*boss_invalid=*/false, /*duration_rounds=*/3),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &forever_node,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE)});

    sbs_srand(2994);
    ScenarioBattle b(make_party(make_pet(0, "窗主", {10001, 10001, 10001, 10001, 10001},
                                         300, 1, 5000,
                                         SoulMark(kMarkWindow, "T6 窗印", ""))),
                     make_party(make_pet(1, "对手", {10001, 10001, 10001, 10001, 10001},
                                         100, 1, 5000)));
    b.start();
    BattleContext& ctx = b.ctx();

    // ── A. 窗恰好 3 回合 ──────────────────────────────────────────────────────
    for (int i = 0; i < 5; ++i) {
        idle_round(b);
    }
    CHECK_EQ(g_window_fires, 3, "T2994 A：3 回合窗节点恰好执行 3 次（R4/R5 静默）");
    CHECK_EQ(g_forever_fires, 5, "T2994 C：永久对照节点 5 轮全跑（零漂移）");

    // ── B. 离场清锚 → 更新器重注册重开窗，再执行恰好 3 次 ────────────────────
    ctx.getPet(0).soulMark.deactivate_soul_mark(&ctx, 0);   // 清锚（T6：再上场重开窗）
    for (int i = 0; i < 6; ++i) {
        idle_round(b);
    }
    CHECK_EQ(g_window_fires, 6, "T2994 B：重开窗后再执行恰好 3 次（累计 6）");
    CHECK_EQ(g_forever_fires, 11, "T2994 B：永久对照不受窗界影响（累计 11）");

    std::cout << "scenario_2994 PASS (T6 魂印窗界一等公民探针)" << std::endl;
    return 0;
}
