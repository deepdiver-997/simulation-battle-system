#include <utils/sbs_rand.h>
// 场景 220：帝皇之钺 3404 第五技「黎明破云钺 30937」缺失效果 1158、1178
//（moves_lib/d5_D6.cpp，夜跑 night-2，2026-09-28）
//
// 1158：自身每处于 1 种能力提升状态附加 50 点固定伤害，遇天敌翻倍（切片 [1,50]）。
// 1178：自身不处于能力提升状态则造成的伤害翻倍（通用增伤 +100%）。
// 断言口径：对手掉血 = raw_attack_damage（翻倍前裸伤台账）+ 期望固伤（粉伤不进台账）。
//
// 断言组：
//   T1 主路径：3 种提升 → 固伤 150、1178 不触发；对手掉血 = 裸伤 + 150。
//   T2 天敌：自身水系 vs 对手草系（草克水）→ 2 种提升固伤 100×2=200。
//   T3 零提升：1158 不发固伤、1178 翻倍 → 对手掉血 = 2 × 裸伤。
//   T4 负等级：只挂下降不算提升（kinds=0）→ 同 T3 形状。
//   T5 链内 913 不干扰：对手无回合类效果 → 913 消除失败无加伤（对 T1 的前提复核）。
//
// 确定性：30937/拍打 crit_rate=0（暴击掷点短路不耗 rand）、双方同系克制 1.0、
// 必中；伤害浮动含在 raw_attack_damage 台账内，等式断言与浮动无关。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWanYun = 30937;  // 黎明破云钺：圣灵战斗物攻 160、必中、effect [913,1158,1178]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果
constexpr int kGrass = 1;       // 草（克水）
constexpr int kWater = 2;       // 水

ElfPet dihuang(int speed, int element = -1) {
    ElfPet p = scenario::make_pet(0, "帝皇之钺", {kWanYun, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    if (element >= 0) {
        p.elementalAttributes[0] = element;
        p.elementalAttributes[1] = 0;
    }
    return p;
}

ElfPet foe(int speed, int element = -1, int hp = 9999) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    if (element >= 0) {
        p.elementalAttributes[0] = element;
        p.elementalAttributes[1] = 0;
    }
    return p;
}

void set_boosts(BattleContext& ctx, int side, int kinds) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[side][i] = i < kinds ? 2 : 0;
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 主路径：3 种提升 → 固伤 150；处于提升 → 1178 不触发 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(dihuang(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_boosts(ctx, 0, 3);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T1 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - 150,
                 "T1 核心：3 种提升 → 附加 150 固伤（无天敌不翻倍）；1178 不触发");
        std::cout << "  [T1] 3 种提升固伤 150 通过（裸伤 " << raw << "）\n";
    }

    //========== T2 天敌：水 vs 草（草克水）→ 固伤翻倍 200 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(dihuang(/*speed=*/200, kWater)),
                         scenario::make_party(foe(/*speed=*/100, kGrass)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_boosts(ctx, 0, 2);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T2 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - 200,
                 "T2 核心：2 种提升 × 50 × 天敌翻倍 = 200 固伤");
        std::cout << "  [T2] 天敌固伤翻倍 200 通过\n";
    }

    //========== T3 零提升：无固伤 + 1178 伤害翻倍 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(dihuang(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T3 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - 2 * raw,
                 "T3 核心：零提升 → 无固伤；1178 翻倍（掉血=2×裸伤）");
        std::cout << "  [T3] 零提升伤害翻倍 通过\n";
    }

    //========== T4 负等级：只挂下降不算提升 → 同 T3 形状 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(dihuang(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;   // 攻击 -2：下降不是提升
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T4 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - 2 * raw,
                 "T4 核心：负等级不计入提升（kinds=0）→ 1158 不发、1178 翻倍");
        std::cout << "  [T4] 负等级不算提升 通过\n";
    }

    //========== T5 对照：非 30937 攻击（拍打）不携带固伤/翻倍链 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(dihuang(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_boosts(ctx, 0, 3);   // 有提升，但本回合用拍打
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5 前提：拍打伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw,
                 "T5 核心：效果只随 30937 走——拍打无固伤无翻倍");
        std::cout << "  [T5] 链随技能走对照 通过\n";
    }

    std::cout << "scenario_220 all passed" << std::endl;
    return 0;
}
