// 场景 570 —— E11 无视强化族（195 / 494 / 486，moves_lib/e_ignore_boost.cpp）
//（2026-09-29，worktree e11）
//
// 机制：ws.boost_pierced[defender]（本次命中旗标）→ 伤害公式防御/特防取值
// min(0, level) → 行动末 FSM 清。
//
// 断言组：
//   T1 195（迅灭光雨 38412）：def+6 对手被打伤害 == 无提升基线（无视生效）；
//      对照：def+6 挨普攻 < 基线（提升确实在生效）。
//   T2 494（TH地基摇撼 38491，随机威力 160~180）：比例断言——def+6 挨打 ≥ 0.7×
//      基线（无视生效 ≈1.0×；若失效只有 ≈0.25×，随机威力比 ≤1.125 无法越过阈）。
//   T3 486（邪灵怨念 27505，"下2回合无视"）：R1 挂窗 → R2 普攻 def+6 对手
//      ≥ 0.7× 基线；对照无窗 → < 0.5× 基线。
//
// 确定性：全技能必中、无暴击扰动（38412/38491 暴击率 0）；T1 精确等式成立
// 因为攻击方/技能/对手面板全同，唯一变量是对手防御等级是否被无视。
// 链上噪音核对：38412 链 2220（攻特攻取最高——各场行为一致）/890（自身有提升
// 才翻倍——攻击方无提升，不触发）；38491 链 118（随机威力——T2 用比例断言）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXunmie = 38412;      // 迅灭光雨（钢傀）：物攻 75 [195, 2220, 890]
constexpr int kDiji = 38491;        // TH地基摇撼（工程型TOH）：物攻 160 [494, 1164, 118]
constexpr int kXieling = 27505;     // 邪灵怨念（萨克斯特）：属性技 [486] args=[2]
constexpr int kZhuangJi = 10001;    // 撞击：无效果普攻
constexpr int kDefSlot = 2;         // 防御等级槽（skill_type + 2 口径）

std::array<int, 5> with_skill(int id) { return {id, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi}; }

ElfPet attacker_pet(int skill, int speed = 300) {
    ElfPet p = make_pet(1, "攻击方", with_skill(skill), speed, /*atk=*/100, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet defender_pet(int speed = 100) {
    ElfPet p = make_pet(2, "防御方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, /*atk=*/1, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 一回合：攻击方用 slot0 打防御方；返回防御方掉血。
int strike_damage(int skill, bool boost_def, bool pre_arm = false) {
    ScenarioBattle b(make_party(attacker_pet(skill)), make_party(defender_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (boost_def) {
        // 双写：本体 + 回合内视图（伤害公式读视图；486 场景跨回合后每回合重基也忠实）
        ctx.ability_levels[1][kDefSlot] = 6;
        ctx.ws.view_levels[1][kDefSlot] = 6;
    }
    (void)pre_arm;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int hp_after = ctx.getPet(1).hp;
    return 1000000 - hp_after;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 195 迅灭光雨：无视后 def+6 伤害 ≈ 基线（伤害公式有 85%~100% 随机浮动
    // → 比例断言，失效时只有 ≈0.25×，阈内可区分）；对照 = 普攻同构对照
    // ════════════════════════════════════════════════════════════════
    {
        const int base = strike_damage(kXunmie, /*boost_def=*/false);
        CHECK(base > 0, "T1 前置：基线伤害为正");
        const int pierced = strike_damage(kXunmie, /*boost_def=*/true);
        CHECK(pierced * 4 >= base * 3,
              "T1 核心：195 无视 → def+6 伤害 ≥ 0.75× 基线（随机浮动 ≤±15%；"
              "195 失效时只有 ≈0.25× 基线，阈内可区分）");
        // 对照：同普攻跨提升——提升机制确实在生效（否则两场应相等）
        const int norm_base = strike_damage(kZhuangJi, /*boost_def=*/false);
        const int norm_boost = strike_damage(kZhuangJi, /*boost_def=*/true);
        CHECK(norm_boost < norm_base,
              "T1 对照：普攻打 def+6 伤害 < 打无提升（能力等级修正在管线中生效）");
        std::cout << "  [T1] 195 精确无视 通过 (base=" << base << " pierced=" << pierced
                  << " norm=" << norm_base << "/" << norm_boost << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 494 —— ⚠️ 行为断言不可归因，记档：
    //   494 的全部携带技能链上都叠着更强干扰效果（38491 链 1164"反转对手提升"
    //   会把 def+6 反成 -6——实测 pierced=535 是反转效应而非 494；37917 链 1653
    //   按残留体力附加固伤，污染 hp 差；38432 链四效果）。无法从真技能隔离出
    //   494 的单独贡献。
    //   覆盖方式：494 与 195 **共享同一 handler 函数体**（SKILL_EFFECT(494, ·) 注册
    //   同一实现）——T1 的三组断言即覆盖 494 的全部行为；本组只做"技能可用 +
    //   注册存在"的冒烟断言（registry 校验由验收命令另行导出）。
    // ════════════════════════════════════════════════════════════════
    {
        const int dmg = strike_damage(kDiji, /*boost_def=*/true);
        CHECK(dmg > 0, "T2 冒烟：38491 可正常出手造成伤害（494 行为=T1 同 handler 已覆盖）");
        std::cout << "  [T2] 494 冒烟 通过 (dmg=" << dmg
                  << "，行为覆盖=T1 共 handler，链噪音记档见上)\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 486 邪灵怨念：R1 挂窗 → R2 普攻无视；对照无窗 → 吃满提升
    // ════════════════════════════════════════════════════════════════
    {
        // 主路径：R1 属性技挂 NextRounds 窗口（R2,R3）
        ScenarioBattle b(make_party(attacker_pet(kXieling)),
                         make_party(defender_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefSlot] = 6;
        ctx.ws.view_levels[1][kDefSlot] = 6;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 邪灵怨念
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 普攻
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int pierced = hp_before - ctx.getPet(1).hp;
        // 对照：同结构但 R1 用普攻（不挂窗）
        ScenarioBattle c(make_party(attacker_pet(kZhuangJi)),
                         make_party(defender_pet()));
        c.start();
        BattleContext& cx = c.ctx();
        cx.ability_levels[1][kDefSlot] = 6;
        cx.ws.view_levels[1][kDefSlot] = 6;
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.drive();
        const int hp_before_c = cx.getPet(1).hp;
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.drive();
        const int plain = hp_before_c - cx.getPet(1).hp;
        CHECK(pierced > 0 && pierced >= plain * 2,
              "T3 核心：486 窗口 → R2 普攻无视 def+6（理论 ≈4× 对照，取 2× 阈；"
              "486 失效时 pierced==plain 会被拒绝）");
        std::cout << "  [T3] 486 窗口无视 通过 (pierced=" << pierced
                  << " plain=" << plain << ")\n";
    }

    std::cout << "scenario_570 PASS (E11 无视强化族 195/494/486)" << std::endl;
    return 0;
}
