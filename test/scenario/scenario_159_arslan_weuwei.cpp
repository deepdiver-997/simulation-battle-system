#include <utils/sbs_rand.h>
// 场景 159：极战·阿尔斯兰 4327 + 无为觉者 4661 技能组（2026-09-27）
//
// 阿尔斯兰：
//   圣裂冲 30066 [1025]       吸取提升 → 回 1/3
//   先驱战拳 30068 [983]      未击败 → 自身全属性+1
//   圣武天战决 30070 [540]    后手 → 下2回合必暴
//   气定神闲 25143 [834]      提升被消除/吸取 → 对手属性技无效 2 回合
//   战魂重临 27856 [1770]     免 1 次攻击伤害 + PP 满条件免粉
//   再战天荒 35650 [1099+799] 消提升→免 1 次异常；回 1/4 + 等量百分比伤害
//   几曾识干戈 35652 [697+1568+1774+1395] 凭证与 ignore_shield（1774/1395 跳过，见文件头）
// 觉者：
//   致虚守静 28777 [599+106]  物理减伤 100% + 特殊攻击必 miss
//   无常夜 37452 [198] / 无晓天 37453 [626] / 无魂归 37455 [1603]
//   滞道销心 28778 [1113]     命中后消除自身提升
//   昊法同愚 28779 [2144]     双方下2回合无法主动切换
//   无兆乱 37456 [38]         最高HP -50
//   无圣显 37457 [1463]       先手免死回满+解异常+清下降
//   孑身诫 37458 [2163+931]   消回合失败→全PP归零；吸强成功→全PP归零
//   千秋虔 37459 [1711+1868+185] 清下降→臣服；对手换宠→臣服 else 束缚；击败臣服者→下只也臣服
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
constexpr int kBind = static_cast<int>(AbnormalStatusId::Bind);
constexpr int kSubmission = static_cast<int>(AbnormalStatusId::Submission);

constexpr int kShengLie = 30066;   // 圣裂冲
constexpr int kXianQu = 30068;     // 先驱战拳
constexpr int kShengWu = 30070;    // 圣武天战决
constexpr int kQiDing = 25143;     // 气定神闲
constexpr int kZhanHun = 27856;    // 战魂重临
constexpr int kZaiZhan = 35650;    // 再战天荒
constexpr int kJiCeng = 35652;     // 几曾识干戈

constexpr int kZhiXu = 28777;      // 致虚守静
constexpr int kWuChang = 37452;    // 无常夜
constexpr int kWuXiao = 37453;     // 无晓天
constexpr int kZhiDao = 28778;     // 滞道销心
constexpr int kWuHun = 37455;      // 无魂归
constexpr int kHaoFa = 28779;      // 昊法同愚
constexpr int kWuZhao = 37456;     // 无兆乱
constexpr int kWuSheng = 37457;    // 无圣显
constexpr int kJieShen = 37458;    // 孑身诫
constexpr int kQianQiu = 37459;    // 千秋虔

ElfPet arslan(int skill, int speed = 200) {
    ElfPet p = scenario::make_pet(0, "极战·阿尔斯兰", {skill, kHit, kHit, kHit, kHit},
                                  speed, 300, 4000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet wuwei(int skill, int speed = 200) {
    ElfPet p = scenario::make_pet(0, "无为觉者", {skill, kHit, kHit, kHit, kHit},
                                  speed, 300, 3000);
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

int ability_sum(const BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_level(side, i);
    }
    return sum;
}

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //═════════════════ 阿尔斯兰 4327 ═════════════════
    //========== T1 圣裂冲 1025：吸强成功 → 回 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kShengLie)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        ctx.getPet(0).hp = 2000;
        use0(b, /*foe_slot=*/0);   // 守方锋利不回打，隔离回血观测
        CHECK_EQ(ctx.ability_level(0, 0), 2, "T1 吸取：攻击提升搬入");
        CHECK_EQ(ctx.getPet(0).hp, 2000 + 4000 / 3, "T1 吸取成功：恢复 1/3 最大体力");
        std::cout << "  [T1] 圣裂冲 1025 通过\n";
    }

    //========== T2 先驱战拳 983：未击败 → 全属性+1 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kXianQu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);
        CHECK(ctx.getPet(1).hp > 0, "T2 前提：未击败");
        CHECK_EQ(ability_sum(ctx, 0), 6, "T2 未击败：全属性+1（六项含命中，总量 6）");
        std::cout << "  [T2] 先驱战拳 983 通过\n";
    }

    //========== T3 圣武天战决 540：后手 → 必暴窗口 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kShengWu, /*speed=*/50)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);   // R1：后手用圣武天战决
        // R2 的 ROUND_START tick 写 must_crit；R3 起窗口过期
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_ROUND_START);
        ctx.debug_step_mode = false;
        CHECK(ctx.ws.must_crit[0], "T3 下2回合必暴（R2 ROUND_START 写入）");
        b.drive();
        std::cout << "  [T3] 圣武天战决 540 通过\n";
    }

    //========== T4 气定神闲 834：提升被消除 → 对手属性技无效 ==========
    // 事件投递口径注：watcher 在 R1 链内的 drain 拿到"提升被清除"事件 → 盔当回合授出；
    // R2 对手属性技（锋利）被拦。（事件在两回合之间的队列要等到后续状态才投递——
    // 834 的消除判定统一放回合链内做，不给跨回合队列留歧义。）
    {
        ScenarioBattle b(scenario::make_party(arslan(kQiDing)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);   // 挂 watcher（4 回合）
        ctx.debug_step_mode = false;
        (void)clear_stat_boosts(&ctx, 0);   // 提升被消除 → 对手属性技封 2 回合
        b.drive();   // R1 链收尾：drain 投递事件 → 盔授出
        // R2：对手用属性技（锋利）→ 被封（无效果落地：攻击等级不变）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_level(1, 0), 0,
                 "T4 提升被消除 → 对手属性技无效（锋利未生效）");
        std::cout << "  [T4] 气定神闲 834 通过\n";
    }

    //========== T5 战魂重临 1770：免 1 次攻击伤害 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kZhanHun)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/0);   // R1 属性技开战魂附体；守方锋利（无攻击，免疫留给 R2）
        // R2 守方拍打 → 攻击伤害被免
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 4000, "T5 免疫对手下1次攻击伤害");
        // R3 免疫已耗 → 拍打落地
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.getPet(0).hp < 4000, "T5 次数型：R3 照常落地");
        std::cout << "  [T5] 战魂重临 1770 通过\n";
    }

    //========== T6 再战天荒 1099+799：消提升→免1次异常；回1/4+等量百分比伤 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kZaiZhan)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        ctx.getPet(0).hp = 1000;   // 低于 1/2 → 效果翻倍：回 2000（满 4000）
        // 单步到本方 SKILL_EFFECT 后断言（守方随后的锋利会自奶 +1，不污染断言）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T6 消除对手能力提升");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T6 低血翻倍：恢复 1/4×2 = 2000");
        CHECK_EQ(ctx.getPet(1).hp, 4000, "T6 等量百分比伤害 2000（SKILL_EFFECT 段）");
        b.drive();   // 再战天荒本体红伤随后落地（6000-红伤）
        // 免 1 次异常：现代施加被挡
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 0, kBurn, 2, 1);
        CHECK(r != ApplyAnomalyResult::SUCCESS, "T6 消除成功 → 免疫下1次异常");
        std::cout << "  [T6] 再战天荒 1099+799 通过\n";
    }

    //========== T7 几曾识干戈：697/1568 凭证 + 1568 无视护盾 ==========
    {
        ScenarioBattle b(scenario::make_party(arslan(kJiCeng)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[0].penetration_flags.ignore_damage_limit,
              "T7 几曾识干戈：697 凭证");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.ws.ignore_shield[0], "T7 1568：本次攻击无视护盾");
        std::cout << "  [T7] 几曾识干戈 697+1568 通过\n";
    }

    //═════════════════ 无为觉者 4661 ═════════════════
    //========== T8 致虚守静：特殊攻击必miss（R1）+ 物理减伤100%（R2）==========
    // R1：守方用吸取（特殊）→ 106 立即 tick（首手）→ hit_rate_mod=0 → 必 miss；
    // R2：守方用拍打（物理）→ 599 窗口 tick → 减伤 100% → 0 落血。
    {
        ElfPet foe = foe_pet();
        foe.skills[2] = Skills(10002);   // 吸取（特殊）给守方
        ScenarioBattle b(scenario::make_party(wuwei(kZhiXu)),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/1);   // R1：守方拍打？——见下，改用槽2
        // 上一行 foe_slot=1 是拍打（物理）→ 599 立即 tick 减伤 → 0 落血
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T8 R1 物理攻击伤害 -100%");
        // R2 守方吸取（特殊）→ 106 窗口 tick → 必 miss
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T8 R2 特殊攻击必 miss");
        std::cout << "  [T8] 致虚守静 599+106 通过\n";
    }

    //========== T9 无常夜/无晓天/无魂归：198/626/1603 ==========
    {
        {
            ScenarioBattle b(scenario::make_party(wuwei(kWuChang)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/2);
            CHECK_EQ(ability_sum(ctx, 1), -3, "T9 198：随机 3 种能力 -1（总量 -3）");
        }
        {
            ScenarioBattle b(scenario::make_party(wuwei(kWuXiao)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/2);
            CHECK_EQ(ability_sum(ctx, 0), 3, "T9 626：随机 3 项能力 +1（总量 3）");
        }
        {
            ScenarioBattle b(scenario::make_party(wuwei(kWuHun)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_pp = ctx.getPet(1).skills[0].pp;
            use0(b, /*foe_slot=*/2);
            CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_pp - 2, "T9 1603：所有PP -2");
        }
        std::cout << "  [T9] 无常夜/无晓天/无魂归 通过\n";
    }

    //========== T10 滞道销心 1113：命中后消除自身提升 ==========
    {
        ScenarioBattle b(scenario::make_party(wuwei(kZhiDao)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 滞道销心（属性技）含"解除自身下降"（effect 3 已实现）+ 1113
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 0, /*特攻=*/1, 2) == StatChangeResult::SUCCESS);
        use0(b);
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T10 解除自身能力下降（effect 3）");
        CHECK_EQ(ctx.ability_level(0, 1), 0, "T10 1113：命中后消除自身提升");
        std::cout << "  [T10] 滞道销心 1113 通过\n";
    }

    //========== T11 昊法同愚 2144：双方无法主动切换 ==========
    {
        ScenarioBattle b(scenario::make_party(wuwei(kHaoFa)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);
        // 主动切换被锁（R2 守方 CHOOSE_PET → 被拒/不生效）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 0, "T11 锁切：守方切换未生效（仍在槽0）");
        std::cout << "  [T11] 昊法同愚 2144 通过\n";
    }

    //========== T12 无兆乱 38：最高HP -50 ==========
    {
        ScenarioBattle b(scenario::make_party(wuwei(kWuZhao)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP], 5950,
                 "T12 战斗时最高HP -50");
        std::cout << "  [T12] 无兆乱 38 通过\n";
    }

    //========== T13 无圣显 1463：先手免死回满+解异常+清下降 ==========
    {
        ElfPet foe = foe_pet();
        foe.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] = 10000;
        ScenarioBattle b(scenario::make_party(wuwei(kWuSheng)),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 0, kBurn, 5, 1) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        ctx.getPet(0).hp = 3000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T13 先手免死：回满");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T13 异常解除");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T13 能力下降解除");
        std::cout << "  [T13] 无圣显 1463 通过\n";
    }

    //========== T14 孑身诫 2163+931：消回合失败→全PP归零 ==========
    {
        ScenarioBattle b(scenario::make_party(wuwei(kJieShen)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 不挂回合类 → 2163 消除失败 → 对手 5 项 PP 归零（931 无提升 → 只走 2163）
        use0(b, /*foe_slot=*/2);
        int zeroed = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK(zeroed >= 5, "T14 2163：对手所有技能 PP 归零（实际归零槽 " << zeroed << "）");
        std::cout << "  [T14] 孑身诫 2163 通过\n";
    }

    //========== T15 千秋虔 1711+1868+185 ==========
    {
        ScenarioBattle b(scenario::make_party(wuwei(kQianQiu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 1711：自身有下降 → 解除成功 → 100% 臣服
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 1, 1) == StatDropResult::SUCCESS);
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T15 自身下降解除");
        CHECK(ctx.has_active_abnormal_status(1, kSubmission), "T15 解除成功 → 对手臣服");
        std::cout << "  [T15] 千秋虔 1711+1868+185 通过（1868/185 见下）\n";
    }
    {
        // 1868：对手当回合换宠（CHOOSE_PET）→ 100% 臣服；else 束缚
        ScenarioBattle b(scenario::make_party(wuwei(kQianQiu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 守方本回合 CHOOSE_PET（原地 no-op 换宠）→ roundChoice[1][0]==0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kSubmission),
              "T15 1868：对手当回合切换精灵 → 臣服");
        std::cout << "  [T15b] 千秋虔 1868 切换分支 通过\n";
    }
    {
        // 185：击败臣服的对手 → 下只出场的对手也臣服
        ScenarioBattle b(scenario::make_party(wuwei(kQianQiu)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, kSubmission, 9, 1) == ApplyAnomalyResult::SUCCESS);
        ctx.getPet(1).hp = 1;   // 残血对手（臣服中）
        use0(b, /*foe_slot=*/2);   // 击败
        CHECK(ctx.getPet(1).hp <= 0, "T15 前提：臣服的对手被击败");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kSubmission),
              "T15 185：下一个出场的对手也进入臣服状态");
        std::cout << "  [T15c] 千秋虔 185 通过\n";
    }

    std::cout << "scenario_159 all passed" << std::endl;
    return 0;
}
