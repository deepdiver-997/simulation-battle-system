#include <utils/sbs_rand.h>
// 场景 6070 —— J274 回合效果被消除减伤窗族（1886，moves_lib/j_break_effect_reduce_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6070-6079 → T6070.a-c 三块）
//
// 断言口径（双局同种子对拍）：
//   - 载体 无终痴念 28439 [1886]|[5,2,50]（纯链；窗 5 回合、下 2 回合、减伤 50%）。
//   - R1 施放 1886 挂 watcher；R2 前给自身挂一张回合型 SEAL 票（source=自身），
//     对手七星碎岳斩 30556 [1032,30]|[1] 走真实断回合（EVENT_BREAK target=自身）。
//   - R3 对手拍打（must_hit）打自身：srand 重置对齐后，断回合局掉血 == 对照局 ×50%
//     （REDUCE 加算槽 50，红伤管线；整数截断按同式核对）。
//   - 审计 991886000+0 = 1（断回合触发）/0（对照）；991886100+0 = 50。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kWuZhong = 28439;   // 无终痴念：1886=[5,2,50] 纯链
constexpr int kQiXing  = 30556;   // 七星碎岳斩：1032=[1]（断回合）
constexpr int kPaiDa   = 10004;   // 拍打：攻击 filler
constexpr int kMaxHp   = 99999;
constexpr int kSeed    = 20266070;

constexpr int kBreakBase = 991886000;   // +owner：断回合触发次数
constexpr int kPctBase = 991886100;     // +owner：最近减伤百分比

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kWuZhong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kQiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

// R1 施 1886；R2 自方造回合效果 + 对手断回合（或对照 NONE）；R3 对手拍打。
// 返回 R3 自身掉血量。
int run_case(bool break_it, int& break_count, int& pct_audit) {
    ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    b.ctx().seed_rng(kSeed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 1886
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();

    // R2 前置：给自身造一个"回合类效果"（SEAL 回合型票，绑定方=自身=source 0），
    // 让对手的 1032 走真实断回合路径（1032 的前置门 = rule_center has_round_type(target)）。
    REQUIRE(core_api().seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/999801,
                                  /*attribute=*/true, /*attack=*/false, /*count=*/1,
                                  /*duration_rounds=*/2, /*penetrable=*/true,
                                  /*source_slot=*/-1, EffectScope::ON_STAGE,
                                  /*hit_invalid=*/false, /*chance_pct=*/100,
                                  /*consumed_when_pierced=*/false) > 0);
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    if (break_it) {
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 1032 断回合
    } else {
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    }
    b.drive();

    break_count = audit_value(ctx, kBreakBase + 0);
    pct_audit = audit_value(ctx, kPctBase + 0);

    const int hp_before = ctx.getPet(0).hp;
    b.ctx().seed_rng(kSeed);   // R3 攻防双方 rand 序对齐（同种子对拍）
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
    b.drive();
    return hp_before - ctx.getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6070.a 断回合触发：审计计数 1、pct 50 ══
    int break_count = 0;
    int pct_audit = 0;
    const int loss_break = run_case(true, break_count, pct_audit);
    CHECK_EQ(break_count, 1, "T6070.a 1886：断回合触发 1 次");
    CHECK_EQ(pct_audit, 50, "T6070.a 1886：审计减伤 50");

    // ══ T6070.b 对照（不断回合）：计数 0、无减伤 ══
    int ctrl_count = 0;
    int ctrl_pct = 0;
    const int loss_ctrl = run_case(false, ctrl_count, ctrl_pct);
    CHECK_EQ(ctrl_count, 0, "T6070.b 1886：未断回合 → 不触发");
    CHECK(loss_ctrl > 0, "T6070.b 前置：对照局 R3 受到红伤");

    // ══ T6070.c 减伤比例：断回合局 == 对照局 ×50%（整数截断）══
    CHECK_EQ(loss_break, loss_ctrl * 50 / 100,
             "T6070.c 1886：R3 掉血 " + std::to_string(loss_break) + " vs 对照 " +
                 std::to_string(loss_ctrl) + "（期望 ×50%）");
    std::cout << "  [T6070] 1886 断回合减伤窗 通过 (break=" << loss_break
              << " ctrl=" << loss_ctrl << ")\n";

    std::cout << "scenario_6070 PASS (J274 回合效果被消除减伤窗族)" << std::endl;
    return 0;
}
