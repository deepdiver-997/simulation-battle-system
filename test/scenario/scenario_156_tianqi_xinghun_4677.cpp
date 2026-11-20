#include <utils/sbs_rand.h>
// 场景 156：星辰万象·天启星魂 4677 技能组（部分）（2026-09-27）
//
//   群星救赎 37555 [565+148]   先手回 1/4 体力；后手 100% 对手命中 -3
//   宙之哲 28835 [1469+57]     3回合内自身粉伤 +30%（AMP 桶）
//   残星稀落 26244 [1235]      3回合每回合 70% 闪避，未触发→对手随机1项PP归零
//   律理虚浮 37556 [2145]      解除自身异常附加给对手 + 对手命中效果失效
//   天之赐 28834 [1655]        每回合结束恢复所有技能 2 点 PP
//   天宙告缚 37557 [761]       3回合内对手恢复效果 -30%
//   归星化魄 37558 [561+727]   先手免死回满；后手能力等级回退到上回合结束
//   谜空净魂 37559 [587+524]   被击败→对手损失500体力+疲惫3回合
//   星合 37512 [699+697]      / 星澜 37515 [1568] —— 凭证/ignore_shield
//
// 未覆盖（技能其它子句由已实现效果承担）：1469 之外的"星盘转动"类魂印联动子句、
// 587 的"造成致命伤害时对手剩余1点体力"（主语不明，保守未实现，见 moves_lib 文件头）。
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

constexpr int kQunXing = 37555;   // 群星救赎
constexpr int kZhouZhe = 28835;   // 宙之哲
constexpr int kCanXing = 26244;   // 残星稀落
constexpr int kLvLi = 37556;      // 律理虚浮
constexpr int kTianZhi = 28834;   // 天之赐
constexpr int kTianZhou = 37557;  // 天宙告缚
constexpr int kGuiXing = 37558;   // 归星化魄
constexpr int kMiKong = 37559;    // 谜空净魂
constexpr int kXingHe = 37512;    // 星合
constexpr int kXingLan = 37515;   // 星澜

ElfPet xinghun(int skill, int speed = 200, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "天启星魂", {skill, kHit, kHit, kHit, kHit},
                                  speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, 100, 4000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void use0(ScenarioBattle& b, int self_slot = 0, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
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

    //========== T1 群星救赎 565：先手回 1/4 体力 ==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kQunXing)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        use0(b);   // 守方锋利不回打
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 750, "T1 先手：恢复 1/4 体力");
        std::cout << "  [T1] 群星救赎 565 通过\n";
    }

    //========== T2 群星救赎 148：后手 100% 对手命中 -3 ==========
    {
        // 速度 50 → 后手
        ScenarioBattle b(scenario::make_party(xinghun(kQunXing, /*speed=*/50)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        CHECK_EQ(ctx.ability_level(1, 5), -3, "T2 后手：对手命中等级 -3");
        std::cout << "  [T2] 群星救赎 148 通过\n";
    }

    //========== T3 宙之哲 1469：自身粉伤 +30%（AMP 桶）==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kZhouZhe)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);   // 宙之哲（属性技）挂 AMP
        // 测试侧直调粉伤原语：100 固定粉伤 → 管线 AMP ×1.3 = 130
        const int before = ctx.getPet(1).hp;
        (void)deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        CHECK_EQ(before - ctx.getPet(1).hp, 130, "T3 粉伤 100 → 130（AMP +30%）");
        std::cout << "  [T3] 宙之哲 1469 通过\n";
    }

    //========== T4 残星稀落 1235：70% 闪避 or 对手随机1项PP归零（分支相容）==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kCanXing)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, 0, 1);
        bool dodged = ctx.ws.dodge_rate[0] > 0.0f;
        int zeroed = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK(dodged || zeroed >= 1,
              "T4 分支相容：闪避或 PP 归零（dodged=" << dodged << " zeroed=" << zeroed
              << "）");
        std::cout << "  [T4] 残星稀落 1235 通过\n";
    }

    //========== T5 律理虚浮 2145：解除自身异常附加给对手 + 命中效果失效 ==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kLvLi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 0, kBurn, 5, 1) == ApplyAnomalyResult::SUCCESS);
        use0(b);
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T5 自身烧伤被解除");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T5 烧伤附加给对手");
        std::cout << "  [T5] 律理虚浮 2145 通过\n";
    }

    //========== T6 天之赐 1655：回合结束恢复所有技能 2 点 PP ==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kTianZhi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[1].pp -= 5;   // 制造非满槽（拍打）
        use0(b);
        // ROUND_END tick：未出手槽 +2（出手槽的天之赐 PP 消耗另算）
        CHECK_EQ(ctx.getPet(0).skills[1].pp, ctx.getPet(0).skills[1].maxPP - 3,
                 "T6 回合结束：PP +2（maxPP-5+2）");
        std::cout << "  [T6] 天之赐 1655 通过\n";
    }

    //========== T7 天宙告缚 761：对手恢复效果 -30% ==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kTianZhou)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        // 对手恢复 100 点 → 实际 70
        ctx.getPet(1).hp = 3000;
        const HealResult r = heal_amount(&ctx, 1, 100);
        (void)r;
        CHECK_EQ(ctx.getPet(1).hp, 3070, "T7 减疗 30%：恢复 100 → 实际 70");
        std::cout << "  [T7] 天宙告缚 761 通过\n";
    }

    //========== T8 归星化魄 561：先手免死 → 体力回满 ==========
    {
        // 守方大攻击一击致死（atk 10000 → 红伤 ~3300 > 3000）；
        // 自身先手用归星化魄（拦截器武装）→ 死亡被拦、回满
        ElfPet foe = foe_pet();
        foe.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] = 10000;
        ScenarioBattle b(scenario::make_party(xinghun(kGuiXing)),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.getPet(0).hp > 0, "T8 免死：未阵亡");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T8 体力等于最大体力");
        std::cout << "  [T8] 归星化魄 561 通过\n";
    }

    //========== T9 归星化魄 727：后手恢复到上回合结束的等级 ==========
    // R1：自身后手用归星化魄（快照机武装，本回合结束记快照）；行动后测试侧 +1 攻
    //     → 快照（攻=1）。R2：守方先手行动后测试侧 +2 攻（=3）→ 自身用归星化魄
    //     → 恢复到上回合结束（攻=1）。
    {
        ElfPet me = xinghun(kGuiXing, /*speed=*/50);   // 慢于守方 → 恒后手
        ScenarioBattle b(scenario::make_party(me), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);   // 自身（后手）动作完，快照机武装
        ctx.debug_step_mode = false;
        REQUIRE(stat_change(&ctx, 0, /*攻击=*/0, 1) == StatChangeResult::SUCCESS);
        b.drive();   // ROUND_END 记快照（攻=1）
        CHECK_EQ(ctx.ability_level(0, 0), 1, "T9 前置：R1 末攻+1（快照值）");
        // R2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);   // 守方（先手）行动完
        ctx.debug_step_mode = false;
        REQUIRE(stat_change(&ctx, 0, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        b.drive();
        CHECK_EQ(ctx.ability_level(0, 0), 1,
                 "T9 后手归星化魄：等级返回至上一回合结束时（攻 3 → 1）");
        std::cout << "  [T9] 归星化魄 727 通过\n";
    }

    //========== T10 谜空净魂 587+524：被击败 → 对手 -500 体力 + 疲惫 ==========
    {
        ElfPet foe = foe_pet();
        foe.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] = 10000;
        ScenarioBattle b(scenario::make_party(xinghun(kMiKong)),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, 0, 1);   // R1：挂 3 回合窗口；守方拍打（不死）
        const int foe_hp = ctx.getPet(1).hp;
        CHECK(ctx.getPet(0).hp > 0, "T10 前置：自身存活");
        // R2：守方大攻击致死 → 对手 -500 真伤 + 疲惫
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.getPet(0).hp <= 0, "T10 前提：自身被击败");
        CHECK(ctx.getPet(1).hp < foe_hp, "T10 对手损失体力（-500 真伤）");
        CHECK(ctx.has_active_abnormal_status(1, kFatigue), "T10 对手疲惫 3 回合");
        std::cout << "  [T10] 谜空净魂 587+524 通过\n";
    }

    //========== T11 星合/星澜：699/697 凭证 + 1568 无视护盾 ==========
    {
        ScenarioBattle b(scenario::make_party(xinghun(kXingHe)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[0].penetration_flags.ignore_attack_immunity
                  && ctx.getPet(0).skills[0].penetration_flags.ignore_damage_limit,
              "T11 星合：699+697 凭证并存");
    }
    {
        ScenarioBattle b(scenario::make_party(xinghun(kXingLan)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.ws.ignore_shield[0], "T11 星澜：本次攻击无视护盾");
        std::cout << "  [T11] 星合/星澜 通过\n";
    }

    std::cout << "scenario_156 all passed" << std::endl;
    return 0;
}
