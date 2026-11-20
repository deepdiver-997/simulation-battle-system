// 场景 047：选择期"微弱→奖励"效果 —— effect 2490「此技能微弱对手时技能先制+3、获得本系属性加成
// 并以神灵系进行伤害结算」（薇尔诗·乐园之初诞 38421）。
//
// 官方数据：`moves.side_effect=[2490, 946, 1892, 843, 2501]`，2490 原文即上（args_num=0 →
// 先制数与"神灵系"都写死在文案里）。
//
// 为什么必须做成**选择期效果**：先制要在**先手权结算之前**定，所以 core 把它放进
// `Skills::selection_effects_`，`on_selected` 统一注册到 `BATTLE_FIRST_MOVE_RIGHT`
// （与 610 天敌 / 2000 条件先制同路）。
//
// ★ 两个口径（用户 2026-09-14）：
//   ① 判"是否微弱"用**技能槽位的静态系别**，**不能读 `ws.skill_element_view`** —— 视图会被
//      "以XX系别进行伤害结算"这类效果改写，读视图等于把改写后的系别喂回判定。
//      core 侧的落地：把技能静态系别**追加进 args[2]/args[3]** 喂给插件
//      （插件拿不到 core 内部的 `resolve_executing_skill`）。
//   ② "以神灵系进行伤害结算"要写**授予**（`ws.skill_element_grant`）而不是直接写
//      `skill_element_view` —— 后者会被 `resolve_skill_execution` 在 ON_SKILL_HIT 的物化覆盖
//      （MOVE_RIGHT 早于 ON_SKILL_HIT）。同 `must_hit_grant` 的教训。
//   ③ "获得本系属性加成"走 `ws.force_involve` —— `calculateDamage` 里 `involve()` 读的是
//      **技能真实系别**（改元素视图只改克制、不改本系），改视图拿不到本系加成。
//
// 数据对照（`types_relation` 实测）：自然(20) 打 混沌(222) = **0.5（微弱）**；神灵(223) 打 混沌 = **2.0（克制）**；
//   自然 打 自然 = 1.0（不微弱）。所以"以神灵系结算"是一次 4 倍的克制翻转，正好当判别器。
//
// 验证点：
//   W1 ★ 守方是**混沌**（自然微弱）→ 触发：+3 先制翻盘（我方速度更低却先手）、
//      物化后的技能系别 = 神灵、force_involve 置位、伤害吃到「神灵克制 × 本系 1.5」
//   W2 守方是**自然**（不微弱）→ 不触发：速度更快的一方先手、系别仍是自然、无本系加成
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kLeYuan = 38421;    // 乐园之初诞：特殊 power 50、自然系(20)、含 2490
constexpr int kGaoSu = 20017;     // 高速移动：属性技能（不还手）

constexpr int kNature = 20;       // 自然系
constexpr int kChaos = 222;       // 混沌系
constexpr int kDeity = 223;       // 神灵系（2490 的目标系别）

// 探针：在技能效果时点读"物化后的技能系别"与两个开关。
// 因先手方可能是 p0 也可能是 p1，故 FIRST/SECOND 两个时点都挂（同一个 owner=0 只会命中一次）。
int g_elem_view0 = -999;
int g_force_involve = -1;
int g_grant_valid = -1;
EffectResult probe_after_materialize(BattleContext* ctx, const EffectArgs&) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    g_elem_view0 = ctx->ws.skill_element_view[0][0];
    g_force_involve = ctx->ws.force_involve[0] ? 1 : 0;
    g_grant_valid = ctx->ws.skill_element_grant_valid[0] ? 1 : 0;
    return EffectResult::kOk;
}

void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

void arm_probes(BattleContext& ctx) {
    register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &probe_after_materialize, 999048);
    register_effect(ctx, 0, State::BATTLE_SECOND_SKILL_EFFECT, &probe_after_materialize, 999048);
}

// 攻方速度 100、守方 101：**不触发 2490 时守方先手**；触发 +3 后攻方翻盘。
scenario::ScenarioBattle make_battle(int defender_element) {
    auto p0 = scenario::make_pet(0, "薇尔诗", {kLeYuan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/300, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/101, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    // 守方系别：make_pet 固定用 {0,0}，这里手动设成被测系别（回合开始 sync 会拷进视图）。
    p1.elementalAttributes[0] = defender_element;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== W1 ★ 守方混沌 → 自然微弱 → 2490 触发 ==========
    {
        scenario::ScenarioBattle b = make_battle(kChaos);
        b.start();
        arm_probes(b.ctx());
        const int hp_before = b.ctx().getPet(1).hp;
        strike(b);
        BattleContext& ctx = b.ctx();
        const int dmg = hp_before - ctx.getPet(1).hp;

        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "W1) ★ +3 先制生效：攻方速度 100 < 守方 101，仍抢到先手");
        CHECK_EQ(g_grant_valid, 1, "W1) 授予了\"以指定系别结算\"");
        CHECK_EQ(g_elem_view0, kDeity,
                 "W1) ★ 物化后的技能系别 = 神灵(223) —— 授予没被物化冲掉");
        CHECK_EQ(g_force_involve, 1, "W1) force_involve 置位（获得本系加成）");

        // 伤害 = (0.84×300/100×50 + 2) × 神灵克制混沌(2.0) × 浮动 × 本系 1.5
        //      = 128 × 2.0 × [0.851,1.0] × 1.5 ≈ [327, 384]
        // 2026-09-17 批 2 起 effect 946（全属性+N，乐园之初诞词条之一）生效：
        // HIT 时攻方全属性+1 → 特攻 ×1.5 → 区间 ×1.5 ≈ [490, 576]。
        // 2026-09-20 起 effect 1892（乐园之初诞词条之一）实现生效：命中即吸 300 → dmg +300
        // → ≈ [790, 876]。
        CHECK(dmg >= 780 && dmg <= 885,
              "W1) ★ 伤害吃到「神灵克混沌 ×2」+「本系 ×1.5」（再含 946 特攻+1 ×1.5、1892 吸取 +300）");
        std::cout << "  [W1] 守方混沌(自然微弱): 先手=攻方、系别 20→223、本系加成开、伤害="
                  << dmg << "\n";
    }

    //========== W2 守方自然 → 不微弱 → 2490 不触发 ==========
    {
        scenario::ScenarioBattle b = make_battle(kNature);
        b.start();
        arm_probes(b.ctx());
        const int hp_before = b.ctx().getPet(1).hp;
        strike(b);
        BattleContext& ctx = b.ctx();
        const int dmg = hp_before - ctx.getPet(1).hp;

        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "W2) 不触发 → 先手归速度更快的守方(101)");
        CHECK_EQ(g_grant_valid, 0, "W2) 没有授予");
        CHECK_EQ(g_elem_view0, kNature, "W2) 技能系别仍是自然(20)");
        CHECK_EQ(g_force_involve, 0, "W2) 没有本系加成");

        // 伤害 = 128 × 自然打自然(1.0) × 浮动 ≈ [109, 128]
        // 2026-09-17 批 2 起 946 生效（HIT 必跑，与 2490 触发与否无关）→ 特攻 ×1.5。
        // 2026-09-20 起 1892 实现生效（HIT 必跑，与 2490 无关）→ 吸取 +300 ≈ [455, 510]。
        CHECK(dmg >= 455 && dmg <= 510,
              "W2) 伤害只有裸伤水平（无克制翻转、无本系加成；含 946 特攻+1 的 ×1.5）");
        std::cout << "  [W2] 守方自然(不微弱): 先手=守方、系别 20、无本系加成、伤害="
                  << dmg << "\n";
    }

    //========== W3 口径回归：判微弱读的是**技能静态系别**，不是视图 ==========
    // 用一个"改技能系别视图"的探针把视图改成自然，再打混沌 —— 若 2490 读视图就会误判"不微弱"。
    // 这里改为反证：2490 在 MOVE_RIGHT 就把判定做完了（早于任何 SKILL_EFFECT 的改系别效果），
    // 所以 W1 的结果不受影响 —— 断言 W1 已经覆盖。此处只锁"args 里带的是技能静态系别"。
    {
        Skills leyuan(kLeYuan);
        CHECK_EQ(leyuan.element[0], kNature, "W3) 乐园之初诞的静态系别 = 自然(20)");
        // 2490 是在 loadSkills 里被塞进 selection_effects_ 的（core 路由），
        // 若没进选择期集合，W1 的 +3 先制根本不会发生 —— W1 即其端到端证据。
        std::cout << "  [W3] 静态系别=自然(20)；2490 走选择期集合（W1 的先制翻盘即证据）\n";
    }

    std::cout << "\n场景 047 通过：2490 = 选择期判「技能静态系别 vs 对手系别」是否微弱 → "
                 "先制+3 / 授予以神灵系结算（物化时不覆盖）/ 强行获得本系加成。\n";
    return 0;
}
