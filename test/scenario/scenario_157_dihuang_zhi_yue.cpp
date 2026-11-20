#include <utils/sbs_rand.h>
// 场景 157：帝皇之钺 3404 技能组（2026-09-27）
//
//   圣光普照 12904 [1]      给予对方损伤的一半会回复自己的体力
//   无尽曙光 25575 [1140]   3回合内对手使用攻击技能则疲惫（191/849/738 已实现）
//   卸甲归钺 25576 [877]    先出手时全属性+1（585/1005/843 已实现）
//   涤罪之锋 30932 [1207]   双倍吸取提升 + 吸取成功对手下1回合先制-3
//   圣龙甩尾 30934 [1171]   未击败对手 → 下1回合受伤 -300 点
//
// 效果 1 的⚠️人工确认结论：模板"给予对方损伤的一半会恢复自己的体力"，引用方为
// 吸取/强力吸取/超级吸取等 12156 个技能——是**真实效果**（经典吸血半血族），非占位符。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;
constexpr int kBuff = 20265;
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

constexpr int kShengGuang = 12904;  // 圣光普照
constexpr int kWuJin = 25575;       // 无尽曙光
constexpr int kXieJia = 25576;      // 卸甲归钺
constexpr int kDiZui = 30932;       // 涤罪之锋
constexpr int kShengLong = 30934;   // 圣龙甩尾

ElfPet yue(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "帝皇之钺", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, 100, 6000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void use0(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 圣光普照 effect 1：损伤一半回自己 ==========
    {
        ScenarioBattle b(scenario::make_party(yue(kShengGuang)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1500;
        use0(b);
        const int dealt = 6000 - ctx.getPet(1).hp;
        CHECK(dealt > 0, "T1 前提：打出红伤");
        CHECK_EQ(ctx.getPet(0).hp, 1500 + dealt / 2, "T1 损伤的一半回复自己");
        std::cout << "  [T1] 圣光普照 effect 1 通过（dealt=" << dealt << "）\n";
    }

    //========== T2 无尽曙光 1140：对手使用攻击技能则疲惫 ==========
    {
        ScenarioBattle b(scenario::make_party(yue(kWuJin)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);   // R1 挂 3 回合窗口；守方锋利（属性技 → 不触发）
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue), "T2 对照：属性技不触发");
        // R2 守方拍打（攻击技）→ ROUND_END tick 疲惫
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kFatigue), "T2 对手使用攻击技 → 疲惫");
        std::cout << "  [T2] 无尽曙光 1140 通过\n";
    }

    //========== T3 卸甲归钺 877：先出手全属性+1 ==========
    {
        ScenarioBattle b(scenario::make_party(yue(kXieJia)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        // 877 贡献 +1（守地灵式基值先制不涉能力）；585 也全属性+1（已实现）→ 总 +2
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_level(0, i), 2, "T3 全属性 +2（585 的 1 + 877 的 1）");
        }
        std::cout << "  [T3] 卸甲归钺 877 通过\n";
    }

    //========== T4 涤罪之锋 1207：双倍吸取 + 对手下回合先制-3 ==========
    {
        // 对照：无提升可吸 → 无先制窗口
        int control = -1;
        {
            ScenarioBattle b(scenario::make_party(yue(kDiZui)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
            ctx.debug_step_mode = false;
            control = ctx.ws.preemptive_level[1];
            b.drive();
        }
        ScenarioBattle b(scenario::make_party(yue(kDiZui)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*特攻=*/1, 2) == StatChangeResult::SUCCESS);
        use0(b, /*foe_slot=*/1);
        // 双倍：吸取 +2 → 再 +2 = 自身特攻 +4；对手清零
        CHECK_EQ(ctx.ability_level(0, 1), 4, "T4 双倍吸取：自身特攻 +4");
        CHECK_EQ(ctx.ability_level(1, 1), 0, "T4 对手特攻清零");
        // 下回合对手先制 -3（差值隔离，152 T1 同款）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[1] - control, -3, "T4 对手下回合先制 -3");
        b.drive();
        std::cout << "  [T4] 涤罪之锋 1207 通过（对照 " << control << "）\n";
    }

    //========== T5 圣龙甩尾 1171：未击败 → 下回合受伤 -300 ==========
    {
        ScenarioBattle b(scenario::make_party(yue(kShengLong)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/1);   // R1：攻击（不致死）→ 未击败 → arm；守方拍打（arm 前落地）
        CHECK(ctx.getPet(1).hp > 0, "T5 前提：未击败对手");
        const int hp_after_r1 = ctx.getPet(0).hp;
        CHECK(hp_after_r1 < 3000, "T5 前置：R1 守方攻击落地（窗口尚未生效）");
        // R2：守方拍打 → 管线点数减伤 300 → 实际落血 = max(0, 红伤-300) = 0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r1,
                 "T5 下回合受伤 -300：R2 守方红伤被完全抵消（R1 的 31 不受影响）");
        std::cout << "  [T5] 圣龙甩尾 1171 通过\n";
    }

    std::cout << "scenario_157 all passed" << std::endl;
    return 0;
}
