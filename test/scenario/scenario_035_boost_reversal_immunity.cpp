// 场景 035：反转对手能力提升原语 `stat_boost_reversal` + 免弱 `STAT_DROP` 查询。
//
// 官方 effect 143"使对手的能力提升效果反转成能力下降效果"（雨燕之影 17587 / 黑水啸天破 16041 等）。
//
// 与 `stat_reversal`（反转**自身下降**→提升，增益类）是**两个方向、两面免疫**：
//   - 反转对手**提升** = 把等级往下压 → **弱化类动作** → **第一件事查免弱 STAT_DROP**，
//     免疫则返回 BLOCKED、等级一点不动（用户 2026-09-13 定）。
//   - 不查 STAT_CLEAR（强化保护）：反转是把提升压成下降，与弱化同一原理，可穿。
//
// 验证点：
//   B1 无免弱：+2/+1 → -2/-1（等量反转）+ 视图同步
//   B2 目标本无提升 → NOTHING（不误报成功）
//   B3 免弱：BLOCKED + 等级原样（有 +2 提升也不动）
//   B4 可穿强化保护 STAT_CLEAR（+3 → -3）
//   B5 端到端 雨燕之影(17587, effect 143)：打有 +2 的目标 → 反转成 -2
//   B6 端到端对照：同一技能打**免弱**目标 → 提升保留
//   B7 与 stat_reversal 分工：反转自身下降不受免弱影响（增益方向）
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kAtk = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kSpAtk = static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK);
constexpr int kSkillYuYanZhiYing = 17587;  // 雨燕之影：130 特攻 + effect 143（反转对手提升）
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

// 端到端：我方用雨燕之影打对手（对手可预先带提升/免弱），返回对手攻击等级。
int yuyan_strike(scenario::ScenarioBattle& b, bool grant_immune, bool grant_protect) {
    b.start();
    b.ctx().ability_levels[1][kSpAtk] = 2;
    b.ctx().ws.view_levels[1][kSpAtk] = 2;
    if (grant_immune) {
        grant_immune_to_drop(b.ctx(), 1, 3);
    }
    if (grant_protect) {
        grant_protect_boosts(b.ctx(), 1, 3);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().ability_levels[1][kSpAtk];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== B1 无免弱：等量反转 + 视图同步 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = 2;
        ctx.ability_levels[1][kSpAtk] = 1;
        ctx.ws.view_levels[1][kAtk] = 2;
        ctx.ws.view_levels[1][kSpAtk] = 1;

        const StatReversalResult r = stat_boost_reversal(&ctx, /*target=*/1);
        CHECK(r == StatReversalResult::REVERSED, "B1) 有提升 → 反转成功");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -2, "B1) +2 → -2（等量反转）");
        CHECK_EQ(ctx.ability_levels[1][kSpAtk], -1, "B1) +1 → -1");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], -2, "B1) 视图同步 -2");
        std::cout << "  [B1] 反转对手提升: 攻+2/特攻+1 → "
                  << ctx.ability_levels[1][kAtk] << "/" << ctx.ability_levels[1][kSpAtk] << "\n";
    }

    //========== B2 无提升 → NOTHING ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = -3;  // 只有下降，没有提升
        CHECK(stat_boost_reversal(&ctx, 1) == StatReversalResult::NOTHING,
              "B2) 目标无提升 → NOTHING（不误报成功）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -3, "B2) 下降不受影响（那是 stat_reversal 的方向）");
        std::cout << "  [B2] 目标只有下降 → NOTHING，等级不变\n";
    }

    //========== B3 免弱：BLOCKED + 等级原样 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = 2;
        ctx.ws.view_levels[1][kAtk] = 2;
        grant_immune_to_drop(ctx, /*owner=*/1, /*rounds=*/3);

        CHECK(stat_boost_reversal(&ctx, 1) == StatReversalResult::BLOCKED,
              "B3) 免弱 → BLOCKED（反转失败）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "B3) 有免弱时 +2 提升原样保留");
        std::cout << "  [B3] 免弱目标: 反转 BLOCKED，+2 仍在\n";
    }

    //========== B4 可穿强化保护（STAT_CLEAR）==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtk] = 3;
        ctx.ws.view_levels[1][kAtk] = 3;
        grant_protect_boosts(ctx, /*owner=*/1, /*rounds=*/3);  // 提升无法被消除或吸取

        CHECK(stat_boost_reversal(&ctx, 1) == StatReversalResult::REVERSED,
              "B4) 强化保护挡不住反转（原理同弱化）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -3, "B4) +3 → -3");
        std::cout << "  [B4] 有强化保护: 仍反转，+3 → " << ctx.ability_levels[1][kAtk] << "\n";
    }

    //========== B5/B6 端到端 雨燕之影(17587, effect 143) ==========
    {
        auto atk = scenario::make_pet(0, "雨燕", {kSkillYuYanZhiYing, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/300, /*atk=*/150, /*hp=*/800);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/2000);
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        const int lv = yuyan_strike(b, /*grant_immune=*/false, /*grant_protect=*/false);
        CHECK_EQ(lv, -2, "B5) 雨燕之影 effect 143：对手特攻+2 → -2");
        std::cout << "  [B5] 雨燕之影(143) 打 +2 目标: 特攻 → " << lv << "\n";
    }
    {
        auto atk = scenario::make_pet(0, "雨燕", {kSkillYuYanZhiYing, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/300, /*atk=*/150, /*hp=*/800);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                  kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/2000);
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        const int lv = yuyan_strike(b, /*grant_immune=*/true, /*grant_protect=*/false);
        CHECK_EQ(lv, 2, "B6) 对手免弱 → 提升保留（不被反转）");
        std::cout << "  [B6] 对照组(对手免弱): 特攻仍为 " << lv << "（反转被挡）\n";
    }

    //========== B7 分工回归：反转自身下降不受免弱影响 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kAtk] = -2;
        ctx.ws.view_levels[0][kAtk] = -2;
        grant_immune_to_drop(ctx, /*owner=*/0, /*rounds=*/3);  // 自己有免弱

        CHECK(stat_reversal(&ctx, 0) == StatReversalResult::REVERSED,
              "B7) 反转自身下降是增益类 → 免弱不挡");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 2, "B7) -2 → +2");
        std::cout << "  [B7] 反转自身下降(增益方向): 免弱不挡，-2 → "
                  << ctx.ability_levels[0][kAtk] << "\n";
    }

    std::cout << "\n场景 035 通过：反转对手提升 = 弱化类动作（先查免弱、可穿强化保护）。\n";
    return 0;
}
