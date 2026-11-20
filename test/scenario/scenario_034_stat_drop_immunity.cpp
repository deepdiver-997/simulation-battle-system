// 场景 034：弱化原语 `stat_drop` + 免弱 `ImmunityType::STAT_DROP` 接线。
//
// 原语规则（用户 2026-09-13 定，三条）：
//   ① **先查免弱、无条件**：目标有免弱 → 直接 IMMUNE 失败、等级一点不动。
//      "即使对手身上还有强化也不能降低"——不许把"目标有 +N 提升"当作可抵消弱化的理由。
//   ② **弱化可以突破强化保护**：`STAT_CLEAR`（能力提升无法被消除或吸取）**不查**——
//      原理不同（那个护的是"已有提升被拿走"，弱化是往下压）。
//   ③ **不突破 -6**：压到 -6 为止（钳制），不像 stat_change 那样整次拒绝。
//
// 免弱来源：瀚宇星皇 903 之佑（"免异常+免弱"）等；本场景直接 grant_immunity 模拟。
//
// 验证点：
//   D1 无免弱：正常弱化（本体 + ws 视图同步）
//   D2 免弱：整次失败（IMMUNE）+ 等级不变，**即使目标身上有 +2 提升也不降**
//   D3 免弱只挡弱化：提升（stat_change）照常生效
//   D4 弱化穿强化保护：目标有 STAT_CLEAR，弱化仍成功
//   D5 不突破 -6：-5 再降 2 → -6（部分生效，SUCCESS）；已在 -6 → AT_FLOOR 不变
//   D6 端到端：effect 5 的负 delta（奉献之火 31269 "令对手攻击-2"）走弱化原语，
//      被免弱挡下、等级与视图都不动
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kAtk = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kSkillFengXianZhiHuo = 31269;  // 奉献之火：effect 5×2（对手攻击/特攻 -2）
constexpr int kSkillZhuangJi = 10001;

void grant_immune_to_drop(BattleContext& ctx, int owner, int rounds) {
    ctx.grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/rounds, /*source_id=*/903,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/0, /*source_effect_id=*/903);
}

void grant_protect_boosts(BattleContext& ctx, int owner, int rounds) {
    ctx.grant_immunity(owner, ImmunityType::STAT_CLEAR, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/rounds, /*source_id=*/1960,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/0, /*source_effect_id=*/1960);
}

scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "我方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                             kSkillZhuangJi, kSkillZhuangJi}, 100, 100, 800);
    auto p1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                             kSkillZhuangJi, kSkillZhuangJi}, 100, 100, 800);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== D1 无免弱：正常弱化 + 视图同步 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        const StatDropResult r = stat_drop(&ctx, /*target=*/1, kAtk, /*amount=*/2);
        CHECK(r == StatDropResult::SUCCESS, "D1) 无免弱 → 弱化成功");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -2, "D1) 本体等级应 -2");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], -2, "D1) ws 视图应同步 -2（伤害/先手权读它）");
        std::cout << "  [D1] 弱化 攻击-2: levels=" << ctx.ability_levels[1][kAtk]
                  << " view=" << ctx.ws.view_levels[1][kAtk] << "\n";
    }

    //========== D2 免弱：整次失败——即使目标身上有强化也不降 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = 2;  // 目标身上还有 +2 提升
        ctx.ws.view_levels[1][kAtk] = 2;
        grant_immune_to_drop(ctx, /*owner=*/1, /*rounds=*/3);

        const StatDropResult r = stat_drop(&ctx, 1, kAtk, 2);
        CHECK(r == StatDropResult::IMMUNE, "D2) 有免弱 → 返回 IMMUNE（直接失败）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "D2) 即使有 +2 提升也不能降（等级原样）");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 2, "D2) 视图同样不动");
        std::cout << "  [D2] 免弱 + 目标带 +2: 弱化 IMMUNE，等级仍 " << ctx.ability_levels[1][kAtk]
                  << "（不拿提升抵消）\n";
    }

    //========== D3 免弱只挡弱化：提升照常 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        grant_immune_to_drop(ctx, /*owner=*/1, /*rounds=*/3);
        const StatChangeResult r = stat_change(&ctx, 1, kAtk, +2);  // stat_change 是原始变更，不查免弱
        CHECK(r == StatChangeResult::SUCCESS, "D3) 免弱不影响提升");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "D3) 提升应正常生效");
        std::cout << "  [D3] 免弱目标被提升: +" << ctx.ability_levels[1][kAtk] << "（免弱 ≠ 免提升）\n";
    }

    //========== D4 弱化可穿强化保护（STAT_CLEAR）==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = 3;
        ctx.ws.view_levels[1][kAtk] = 3;
        grant_protect_boosts(ctx, /*owner=*/1, /*rounds=*/3);  // 能力提升无法被消除或吸取

        const StatDropResult r = stat_drop(&ctx, 1, kAtk, 2);
        CHECK(r == StatDropResult::SUCCESS, "D4) 强化保护挡不住弱化（原理不同）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 1, "D4) +3 被弱化 2 → +1");
        std::cout << "  [D4] 有强化保护(STAT_CLEAR): 弱化仍成功，+3 → "
                  << ctx.ability_levels[1][kAtk] << "\n";
    }

    //========== D5 不突破 -6 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = -5;
        ctx.ws.view_levels[1][kAtk] = -5;
        CHECK(stat_drop(&ctx, 1, kAtk, 2) == StatDropResult::SUCCESS, "D5) -5 再降 2 → 部分生效");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -6, "D5) 应钳在 -6（不越界）");
        CHECK(stat_drop(&ctx, 1, kAtk, 1) == StatDropResult::AT_FLOOR, "D5) 已在 -6 → AT_FLOOR");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -6, "D5) 仍是 -6");
        std::cout << "  [D5] -5 降 2 → " << ctx.ability_levels[1][kAtk] << "（封底不越界）\n";
    }

    //========== D6 端到端：effect 5 负 delta 走弱化原语，被免弱挡下 ==========
    {
        auto atk = scenario::make_pet(0, "艾恩斯", {kSkillFengXianZhiHuo, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/300, /*atk=*/150, /*hp=*/3000);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/4000);
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        b.start();
        grant_immune_to_drop(b.ctx(), /*owner=*/1, /*rounds=*/3);  // 对手免弱
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][kAtk], 0, "D6) 免弱 → 奉献之火的攻击-2 不生效");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 0, "D6) 视图也不动");
        std::cout << "  [D6] 奉献之火(31269, effect5) 打免弱目标: 对手攻击等级 "
                  << ctx.ability_levels[1][kAtk] << "（弱化被挡）\n";
    }

    //========== D7 端到端对照：无免弱时同样一击应当弱化成功 ==========
    {
        auto atk = scenario::make_pet(0, "艾恩斯", {kSkillFengXianZhiHuo, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/300, /*atk=*/150, /*hp=*/3000);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/4000);
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][kAtk], -2, "D7) 无免弱 → 攻击-2 正常生效");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], -2, "D7) 视图同步 -2");
        std::cout << "  [D7] 对照组(无免弱): 对手攻击等级 " << ctx.ability_levels[1][kAtk] << "\n";
    }

    //========== D8 自身增益走 stat_change（顺带回归：视图同步）==========
    // 高速移动 20017 = effect 4 [4,100,2]：自身速度 +2。原先插件手抄 clamp 只改本体、
    // 不同步 ws.view_levels → 当回合先手权比较（读视图）看不到这次提速。
    {
        constexpr int kSkillGaoSuYiDong = 20017;
        constexpr int kSpeed = static_cast<int>(NumericalPropertyIndex::SPEED);
        auto p0 = scenario::make_pet(0, "提速方", {kSkillGaoSuYiDong, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi},
                                     /*speed=*/100, /*atk=*/100, /*hp=*/800);
        auto p1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                 kSkillZhuangJi, kSkillZhuangJi},
                                     /*speed=*/200, /*atk=*/100, /*hp=*/800);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][kSpeed], 2, "D8) 自身速度应 +2");
        CHECK_EQ(ctx.ws.view_levels[0][kSpeed], 2, "D8) 视图同步（原先手抄 clamp 漏同步）");
        std::cout << "  [D8] 高速移动 自身速度+2: levels=" << ctx.ability_levels[0][kSpeed]
                  << " view=" << ctx.ws.view_levels[0][kSpeed] << "\n";
    }

    std::cout << "\n场景 034 通过：弱化原语 = 先查免弱(无条件) + 可穿强化保护 + 钳 -6 + 视图同步。\n";
    return 0;
}
