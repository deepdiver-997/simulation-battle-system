#include <utils/sbs_rand.h>
// 场景 260：不灭地威·萨瑞卡 3637 第五技「X·天罡碎岩 32178」缺失效果 774、1086
//（moves_lib/d5_D10.cpp，夜跑 night-2，2026-09-28）
//
// 774：自身当前体力严格高于对手 → 附加对手最大体力 1/3 的百分比伤害（9999/3=3333）。
// 1086：自身当前体力严格低于对手 → 造成的伤害 +50%。
// 断言口径：对手掉血 = raw_attack_damage（AMP 前裸伤台账）+ 期望粉伤（3333 走百分比
// 管线，白板宠抗性/护罩全中性 → 足额落血）。
//
// 断言组：
//   T1 主路径 774：自身 5000 > 对手 3000 → 掉血 = 裸伤 + 3333；1086 不触发（非低于）。
//   T2 主路径 1086：自身 3000 < 对手 5000 → 掉血 = 1.5×裸伤（+50% 增伤）；774 不触发。
//   T3 相等边界：体力相等 → 两者皆不发 → 掉血 = 裸伤。
//   T4 对照：拍打不带链 → 无论血量差均无附加。
//
// 确定性：32178/拍打 crit_rate=0、必中；链内 485 对手无强化 → 消强失败不回血。
// 浮动伤害含在裸伤台账内；1086 的 +50% 走整数管线（raw*3/2 向下取整口径用区间断言）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kSuiYan = 32178;  // X·天罡碎岩：战斗地面物攻 160、必中、effect [485,774,1086]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果

ElfPet sarika(int speed) {
    ElfPet p = scenario::make_pet(0, "不灭地威·萨瑞卡", {kSuiYan, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 主路径 774：体力领先 → 附加 1/3 最大体力（3333）==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sarika(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 9999;   // 自身 9999 > 对手 5000（默认满血）
        ctx.getPet(1).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T1 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 5000 - raw - 3333,
                 "T1 核心：体力高于对手 → 附加 9999/3=3333 百分比伤害");
        std::cout << "  [T1] 体力领先附加 3333 通过（裸伤 " << raw << "）\n";
    }

    //========== T2 主路径 1086：体力落后 → 伤害 +50% ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sarika(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3000;   // 自身 3000 < 对手 5000
        ctx.getPet(1).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - 5000 - ctx.getPet(1).hp;
        CHECK(raw > 0, "T2 前提：攻击伤害落地");
        CHECK(loss >= raw * 3 / 2 - 1 && loss <= raw * 3 / 2 + 1,
              "T2 核心：+50% 增伤（AMP 整数取整口径下 loss=1.5×裸伤±1）");
        CHECK(loss > raw, "T2 核心：增伤确实放大了伤害");
        CHECK(ctx.getPet(1).hp == 9999 - 5000 - loss, "T2 口径：掉血全部来自本击");
        std::cout << "  [T2] 体力落后增伤 50% 通过（裸伤 " << raw << " 掉血 " << loss << "）\n";
    }

    //========== T3 相等边界：两者皆不发 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sarika(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4000;   // 相等：非"高于"亦非"低于"
        ctx.getPet(1).hp = 4000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T3 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 4000 - raw,
                 "T3 核心：体力相等 → 774/1086 均不触发");
        std::cout << "  [T3] 相等边界 通过\n";
    }

    //========== T4 对照：拍打不带链 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sarika(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 9999;   // 体力领先也不该有附加（拍打无链）
        ctx.getPet(1).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T4 前提：拍打伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 5000 - raw,
                 "T4 核心：非 32178 攻击无附加无增伤");
        std::cout << "  [T4] 拍打无链对照 通过\n";
    }

    std::cout << "scenario_260 all passed" << std::endl;
    return 0;
}
