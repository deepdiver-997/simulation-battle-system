// 场景 050：条件盔 —— effect 854「令对手下1次使用的威力高于{0}的攻击技能无效」。
//
// 技能：天叶领域 24560（side_effect=[532,191,854]）/ 余温之辰 26170（[191,1065,854]），
//   两处 854 的参数都是 [120] → 阈值 120。
//
// 建模要点（本场景同时是"2026-09-14 那个待定问题"的答案）：
//   854 是**条件盔** —— 直接复用 `RuleTicket::condition`，而 `RuleCenter::notify` 对
//   **条件不满足的条目是"跳过、不消耗"**。于是三件事同时免费成立：
//     · 对手用威力 ≤120 的攻击技能 → 不拦、**也不扣次数**（下次用大威力技能仍被拦）
//     · 对手用属性技能 → `responds_to(false)` 跳过，同样不消耗
//     · 对手本回合没出手（被控/换宠）→ notify 根本不为它跑，自然不消耗
//   ★ 所以**不需要**当初设想的那套"选完技能后注册当回合狮盔 + 事件撤销选择期监听器"——
//     把判定放进条件、让规则中心在"对手真正用技能时"求值即可，既不白挂也不残留。
//   ★ 判据是 **面板威力**（`notify` 传来的 `Skills::power`），不是 `ws.skill_power_view`——
//     官方口径：威力宝石/变威力类改的是视图，条件盔看面板（用户 2026-09-13）。
//     （结构性保证：门判定发生在 `query_usage`，而视图此刻刚被物化成 `skill.power`，两者相等。）
//
// 验证点：
//   Q1 大威力攻击技能（150 > 120）→ **被拦**；且次数**被消耗**（下一回合同样技能不再被拦）
//   Q2 ★ 小威力攻击技能（35 ≤ 120）→ 不拦、**也不消耗** → 下一回合大威力技能**仍被拦**
//   Q3 ★ 属性技能 → 不消耗 → 下一回合大威力技能**仍被拦**
//   Q4 ★ 边界：威力**恰好 120** 不大于阈值 → 不拦不消耗
//   Q5 穿盔凭证（699）也穿不过去（条件盔 = 龙威一侧，`penetrable=false`）
//   Q6 随切换失效：施放方换宠 → 盔清（scope ON_STAGE）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kBaiLie = 10304;    // 百裂冲击：物理 power **150**（> 阈值 120）
constexpr int kQuanLi = 10033;    // 全力一击：物理 power **120**（边界，不大于阈值）
constexpr int kZhuangJi = 10001;  // 撞击：物理 power **35**（≤ 阈值）
constexpr int kGaoSu = 20017;     // 高速移动：属性技能（power 0）

constexpr int kThreshold = 120;
constexpr int kArmorEffectId = 854;

// p0 = 施放方（挂 854 的那一方），p1 = 攻击方。p1 速度更快（先手）。
scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "天叶方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "攻击方", {kBaiLie, kQuanLi, kZhuangJi, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/200, /*hp=*/3000);
    p1.skills[0].accuracy = 100;
    p1.skills[0].must_hit = true;
    p1.skills[1].accuracy = 100;
    p1.skills[1].must_hit = true;
    p1.skills[2].accuracy = 100;
    p1.skills[2].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在插件中注册");
    e.logic(&ctx, e.args);
}

// 挂 854：source=self(0)、target=对手(1)、阈值 120。
void arm_854(BattleContext& ctx) {
    call_effect(ctx, kArmorEffectId, {0, 1, kThreshold});
}

// p0 当前在场精灵本回合累计掉的血（换宠后是新精灵的，故用"最大体力 - 体力"）。
int pet0_taken(BattleContext& ctx) {
    const ElfPet& pet = ctx.getPet(0);
    return pet.numericalBase[NumericalPropertyIndex::HP] - pet.hp;
}

// 打一回合：p0 用属性技能（不还手），p1 用技能 `p1_skill`。返回 p0 本回合新掉的血。
int round_attack(scenario::ScenarioBattle& b, int p1_skill) {
    const int before = pet0_taken(b.ctx());
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_skill);
    b.drive();
    return pet0_taken(b.ctx()) - before;
}

// p0 换宠（CHOOSE_PET → 槽 1），p1 用技能 `p1_skill`。返回 p0 新精灵掉的血。
int round_switch(scenario::ScenarioBattle& b, int p1_skill) {
    const int before = pet0_taken(b.ctx());
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_skill);
    b.drive();
    return pet0_taken(b.ctx()) - before;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== Q1 大威力攻击技能 → 被拦 + 消耗 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        const int dmg1 = round_attack(b, /*百裂冲击*/ 0);
        CHECK_EQ(dmg1, 0, "Q1) 150 > 120 → 被拦（伤害 0）");
        const int dmg2 = round_attack(b, /*百裂冲击*/ 0);
        CHECK(dmg2 > 0, "Q1) 次数已消耗 → 第二次同样的技能不再被拦");
        std::cout << "  [Q1] 大威力技能: 第1次伤害=" << dmg1 << "（拦下）第2次=" << dmg2
                  << "（次数已消耗）\n";
    }

    //========== Q2 ★ 小威力技能：不拦、也不消耗 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        const int dmg_small = round_attack(b, /*撞击(35)*/ 2);
        CHECK(dmg_small > 0, "Q2) 35 ≤ 120 → 不拦（照常打伤害）");
        const int dmg_big = round_attack(b, /*百裂冲击*/ 0);
        CHECK_EQ(dmg_big, 0, "Q2) ★ 威力不够的技能**不消耗**次数 → 下一步大威力技能仍被拦");
        std::cout << "  [Q2] 小威力技能: 伤害=" << dmg_small
                  << "（不拦）；随后大威力技能伤害=" << dmg_big << "（仍被拦 → 次数没被消耗）\n";
    }

    //========== Q3 ★ 属性技能：不消耗 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        round_attack(b, /*高速移动（属性）*/ 3);
        const int dmg_big = round_attack(b, /*百裂冲击*/ 0);
        CHECK_EQ(dmg_big, 0, "Q3) ★ 属性技能不响应也不消耗 → 下一步大威力技能仍被拦");
        std::cout << "  [Q3] 属性技能不消耗: 随后大威力技能伤害=" << dmg_big << "（仍被拦）\n";
    }

    //========== Q4 ★ 边界：威力恰好 120 不大于阈值 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        const int dmg_120 = round_attack(b, /*全力一击(120)*/ 1);
        CHECK(dmg_120 > 0, "Q4) 120 **不大于** 120 → 不拦（阈值是严格大于）");
        const int dmg_big = round_attack(b, /*百裂冲击*/ 0);
        CHECK_EQ(dmg_big, 0, "Q4) 也不消耗 → 下一步大威力技能仍被拦");
        std::cout << "  [Q4] 边界威力 120: 伤害=" << dmg_120 << "（不拦、不消耗）；随后 150 的="
                  << dmg_big << "（被拦）\n";
    }

    //========== Q5 穿盔凭证（699）穿不过去 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        // p1 的百裂冲击带"无视攻击免疫"（官方 699）——条件盔是龙威一侧（penetrable=false）
        b.ctx().getPet(1).skills[0].penetration_flags.ignore_attack_immunity = true;
        const int dmg = round_attack(b, /*百裂冲击*/ 0);
        CHECK_EQ(dmg, 0, "Q5) 条件盔不可穿（penetrable=false）→ 带穿盔凭证仍被拦");
        std::cout << "  [Q5] 带 699 穿盔凭证: 伤害=" << dmg << "（条件盔不可穿，仍拦下）\n";
    }

    //========== Q6 随切换失效（scope ON_STAGE）==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        arm_854(b.ctx());
        const int dmg = round_switch(b, /*百裂冲击*/ 0);
        CHECK(dmg > 0, "Q6) 施放方换宠 → 盔随切换作废 → 攻击不再被拦");
        std::cout << "  [Q6] 施放方换宠: 伤害=" << dmg << "（盔已随切换清掉）\n";
    }

    std::cout << "\n场景 050 通过：854 条件盔 = RuleTicket::condition + notify 的\"条件不满足即跳过、"
                 "不消耗\"——威力不够的攻击技能/属性技能/本回合没出手 都不会白白吃掉它，"
                 "不需要任何选择期监听器或当回合狮盔。判据是面板威力（严格大于阈值）。\n";
    return 0;
}
