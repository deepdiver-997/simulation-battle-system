#include <utils/sbs_rand.h>
// 场景 064：通用特性批次 D —— 强袭(65 物理版) / 强攻(62) / 强念(63) / 乱舞(503,504)。
//
// 口径来源：new_se.intro/desc（数据库为准）+ 赛学必修6《精灵特性》：
//   - 强袭/精神 同属 Eid 65："物理攻击或者特殊攻击伤害增加 n%"，args[0] 是**官方类别码**
//     （1=物理=强袭 / 2=特殊=精神）、args[1]=百分点；必修6："强袭和精神**不属于通用增伤，乘法计算**"。
//   - 强攻(62)/强念(63)："物理/特殊攻击有 n% 几率使伤害提高 m 点"（args={概率百分点, 点数}）；
//     必修6 ①"属于**追加伤害，与红伤一并结算**"、②"**攻击技能 miss 了也可以扣除对手体力**"。
//   - 乱舞（503/504）：**已被官方废除**（"0 星乱舞跟白板一样"）→ 登记 kind、行为 no-op。
//
// 验证点：
//   D1 强袭 args={1,50} + 物理技能 → ×1.5（AMP_EXTRA 乘法）
//   D2 强袭 + 特殊技能 → 不增伤（类别门控读 args[0]，不是硬编码"只认特殊"）
//   D3 精神（args[0]=2）+ 物理技能 → 不增伤（反向门控）
//   D4 强攻 args={1000,35} + 物理命中 → 伤害 +35 点
//   D5 ★ 强攻 + **miss** → 仍扣 35 点血（必修6 ②）
//   D6 强念（特殊）miss → 同样扣血；强攻打特殊技能 → 不触发
//   D7 乱舞：登记为 kind 但行为 no-op（伤害与无特性对照一致）
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40（accuracy 100）
constexpr int kShuiQiang = 10013;  // 水枪：特攻 40（accuracy 100）
constexpr int kGaoSu = 20017;      // 高速移动：属性技能

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet attacker(CommonTrait trait, int skill) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill},
                              200, 200, 4000, {}, std::move(trait));
}
ElfPet defender(int hp = 4000) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, hp);
}
struct Outcome { int dmg; };
Outcome strike(ScenarioBattle& b) {
    const int before = b.pet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return {before - b.pet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== D1 强袭 + 物理 → ×1.5 ==========
    int base_phys = 0;
    {
        ScenarioBattle ctrl = make_battle(attacker({}, kPaiDa), defender());
        ctrl.start();
        base_phys = strike(ctrl).dmg;
        CHECK(base_phys > 0, "D1 前提：对照组有伤害");

        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强袭", 5, {1, 50}), kPaiDa), defender());
        b.start();
        CHECK_EQ(strike(b).dmg, base_phys * 150 / 100, "D1 强袭应给物理技能 ×1.5");
        std::cout << "D1 PASS 强袭 ×1.5（" << base_phys << " → " << base_phys * 3 / 2 << "）\n";
    }

    //========== D2 强袭不吃特殊技能 ==========
    {
        ScenarioBattle ctrl = make_battle(attacker({}, kShuiQiang), defender());
        ctrl.start();
        const int base = strike(ctrl).dmg;
        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强袭", 5, {1, 50}), kShuiQiang), defender());
        b.start();
        CHECK_EQ(strike(b).dmg, base, "D2 强袭只认物理（打特攻不增伤）");
        std::cout << "D2 PASS 强袭不吃特殊技能\n";
    }

    //========== D3 精神不吃物理技能（反向门控，args[0]=2）==========
    {
        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("精神", 5, {2, 50}), kPaiDa), defender());
        b.start();
        CHECK_EQ(strike(b).dmg, base_phys, "D3 精神只认特殊（打物理不增伤）");
        std::cout << "D3 PASS 精神不吃物理技能\n";
    }

    //========== D4 强攻命中：+35 点 ==========
    {
        ScenarioBattle ctrl = make_battle(attacker({}, kPaiDa), defender());
        ctrl.start();
        const int base = strike(ctrl).dmg;

        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强攻", 5, {1000, 35}), kPaiDa), defender());
        b.start();
        CHECK_EQ(strike(b).dmg, base + 35, "D4 强攻命中应 +35 点");
        std::cout << "D4 PASS 强攻命中 +35 点（" << base << " → " << base + 35 << "）\n";
    }

    //========== D5 ★ 强攻 + miss → 仍扣 35 点（必修6 ②）==========
    {
        ElfPet dodge_def = defender();
        dodge_def.soulMark = scenario::make_dodge_soul_mark();   // 守方闪避 100% → 攻方必 miss
        // 对照：无特性 miss → 0 伤害
        ScenarioBattle ctrl = make_battle(attacker({}, kPaiDa), dodge_def);
        ctrl.start();
        CHECK_EQ(strike(ctrl).dmg, 0, "D5 前提：无特性 miss 无伤害");

        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强攻", 5, {1000, 35}), kPaiDa), dodge_def);
        b.start();
        CHECK_EQ(b.ctx().ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "D5 前提：本次确实 miss（SKILL_INVALID）");
        CHECK_EQ(strike(b).dmg, 35, "D5 ★ miss 也应扣 35 点（追加伤害照常结算）");
        std::cout << "D5 PASS 强攻 miss 也扣血（35 点）\n";
    }

    //========== D6 强念（特殊）miss 也扣血；强攻打特殊技能不触发 ==========
    {
        ElfPet dodge_def = defender();
        dodge_def.soulMark = scenario::make_dodge_soul_mark();

        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强念", 5, {1000, 40}), kShuiQiang), dodge_def);
        b.start();
        CHECK_EQ(b.ctx().ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID, "D6 前提：miss");
        CHECK_EQ(strike(b).dmg, 40, "D6 强念 miss 应扣 40 点");
        std::cout << "D6a PASS 强念 miss 扣血\n";

        // 强攻（物攻特性）打特殊技能 → 类别不符，miss 也不扣
        ScenarioBattle b2 = make_battle(
            attacker(scenario::make_trait("强攻", 5, {1000, 35}), kShuiQiang), dodge_def);
        b2.start();
        CHECK_EQ(b2.ctx().ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID, "D6b 前提：miss");
        CHECK_EQ(strike(b2).dmg, 0, "D6b 强攻不应作用在特殊技能上");
        std::cout << "D6b PASS 强攻不作用于特殊技能\n";
    }

    //========== D8 强攻 + **被盔** → 仍扣 35 点（保底红伤，四个分支都要给）==========
    {
        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强攻", 5, {1000, 35}), kPaiDa), defender());
        b.start();
        // 给守方挂一条"拦攻击技能"的盔（次数 1）→ 攻方本次被无效
        (void)seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70064,
                         /*attribute=*/false, /*attack=*/true, /*count=*/1);
        CHECK_EQ(strike(b).dmg, 35, "D8 被盔也应扣 35 点（保底红伤）");
        std::cout << "D8 PASS 强攻被盔仍扣血（35 点）\n";
    }

    //========== D9 强攻 + **命中效果失效·白板** → 仍扣 35 点 ==========
    {
        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("强攻", 5, {1000, 35}), kPaiDa), defender());
        b.start();
        // "命中效果失效·白板"（kFullNull：效果失效 + 伤害归 0）挂在**承受方**——
        // ③层语义是"防御方按次生效"：条目在防御方，失效的**来袭攻击**的命中效果
        //（消费点是 consume_hit_invalid(1 - owner)，owner = 出手方）。
        hit_effect_invalid(&b.ctx(), /*target=*/1, HitInvalidMode::kFullNull,
                           /*count=*/1, /*is_attribute_skill=*/false);
        const int dmg_null = strike(b).dmg;
        std::cout << "  [D9] 白板下的总伤害 = " << dmg_null
                  << "（exec_result=" << static_cast<int>(b.ctx().ws.skill_exec_result[0]) << "）\n";
        CHECK_EQ(dmg_null, 35, "D9 白板也应扣 35 点（管线伤害被归零，保底照给）");
        std::cout << "D9 PASS 强攻白板仍扣血（35 点）\n";
    }

    //========== D7 乱舞：已废除 → 行为 no-op ==========
    {
        ScenarioBattle ctrl = make_battle(attacker({}, kPaiDa), defender());
        ctrl.start();
        const int base = strike(ctrl).dmg;

        ScenarioBattle b = make_battle(
            attacker(scenario::make_trait("乱舞", 5, {10, 0}), kPaiDa), defender());
        b.start();
        CHECK_EQ(strike(b).dmg, base, "D7 乱舞已废除：行为应与白板一致（no-op）");
        // 顺带确认它被登记成了 kind（而不是 Unknown/None）——便于将来官方复活时接手
        CHECK_EQ(static_cast<int>(trait_kind_from_name("乱舞")),
                 static_cast<int>(TraitKind::Dancer), "D7 乱舞应登记为 Dancer（no-op 占位）");
        std::cout << "D7 PASS 乱舞 no-op（伤害与白板一致）\n";
    }

    std::cout << "\n场景 064 全部通过：特性批次 D（强袭/强攻/强念/乱舞）✓" << std::endl;
    return 0;
}
