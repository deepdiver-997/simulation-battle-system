#include <utils/sbs_rand.h>
// 场景 150：B 组共享效果 id 批产（moves_lib/batch_b_skills.cpp，2026-09-27）
//
// 每个共享效果 id 至少一条**最小构造**主路径断言（不依赖具体精灵）——载体技能用
// mon_id=0 的通用技（side_effect 单效果，隔离被测面）：
//   T1  1   吸取 10002            损伤一半回血（resolvedDamage.final/2）
//   T2  38  龙卷水击 10191        最高HP -10 点（reduce_max_hp_flat）
//   T3  55  属性反转 20113        战斗时属性视图对调（foe 元素 {1,1}）
//   T4  63  相位光波 10859        能力下降镜像反馈 + 自身解除
//   T5  64  铁山靠 10754          烧伤/冻伤/中毒 → damage_add_pct +100
//   T6  66  宝石脉冲 10736        击败 → 回最大体力 1/3
//   T7  67  疲乏撕咬 10778        击败 → 对方下只出战精灵 maxHP -1/3
//   T8  89  蜜桃甜甜弹 12124      每次造成伤害 1/8 回血（watcher）
//   T9  94  石化之视 20731        100% 令对手石化
//   T10 102 电能引爆 11736        对手麻痹 → 威力视图翻倍
//   T11 106 光涌之爪 16528        特殊攻击对自身必定 miss（hit_rate_mod=0）
//   T12 128 异界之光 21174        受到的攻击伤害转化为自身体力（净增口径）
//   T13 155 打盹 21422            回满 + 清下降 + 自身睡眠
//   T14 165 光影冲击 24781        每回合防御/特防 +1（首手立即 1 次 + 窗口）
//   T15 185 火云狂战破 15179      击败烧伤的对手 → 下只出场对手也烧伤
//   T16 198 八卦断念斩 16475      随机 1 种能力 -1（等级总和断言）
//   T17 440 螺旋爆裂拳 15591      对手 PP 消耗变 3 倍
//   T18 599 撼地断岩拳 16384      受特殊攻击伤害 -25%（damage_reduce_mul 槽）
//   T19 620 威慑镇压 31647        致命一击率 +7/16（crit_rate_add）
//   T20 626 对影独酌 19569        随机 2 项能力 +1（等级总和断言）
//   T21 660[0] 蛮横重锤 30938     消除对手能力提升成功 → 恢复 100% 体力
//   T22 660[1] 桓侯震天击 16373   解除自身能力下降成功 → 恢复 100% 体力
//   T23 931 致命无存击 19668      吸取提升成功 → 对手随机 1 项技能 PP 归零
//
// 确定性：sbs_srand(20260927) 固定序列；所有载体技能场景侧统一 must_hit（凭证短路不掷
// 命中）；暴击率 0（harness 默认清零、判定短路）。概率类（94/185 的施加）用 100% 档；
// 随机槽类（198/626/931）断言总量/存在性、不断言具体槽。
#include <cstdlib>
#include <cmath>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;      // 拍打：物攻 40
constexpr int kSpecial = 10002;  // 吸取：特攻 20（兼 T1 载体）
constexpr int kBuff = 20265;     // 锋利：自身攻击+1（铺提升用）

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);
constexpr int kPetrify = static_cast<int>(AbnormalStatusId::Petrify);
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

// 通用测试宠：技能全必中（载体命中率不掷骰）；速度快于对手 → 恒先手。
ElfPet attacker(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "攻方", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 守方技能**不置 must_hit**（accuracy=100 → roll 恒中、确定性不变）：
// T11 靠 must-hit 绕过"必定miss"是官方口径（必中治不了必miss），守方必须可被 miss。
ElfPet foe_pet(int hp = 4000, int atk = 100) {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kSpecial, kHit, kHit, kHit},
                                  /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void use_skill0(ScenarioBattle& b, int foe_slot = 1) {
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

    //========== T1 effect 1 吸取：损伤一半回自己的体力 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(kSpecial)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2500;   // 留出回血空间
        use_skill0(b, /*foe_slot=*/0);   // 守方用锋利（属性技，不产生新的伤害结算）
        // 攻方吸取的红伤从守方掉血反推（ws 快照会被后续技能执行清掉，不可事后读）
        const int dealt = 4000 - ctx.getPet(1).hp;
        CHECK(dealt > 0, "T1 前提：吸取打出红伤");
        CHECK_EQ(ctx.getPet(0).hp, 2500 + dealt / 2, "T1 吸取：恢复损伤的一半");
        std::cout << "  [T1] effect 1 通过（dealt=" << dealt << "）\n";
    }

    //========== T2 effect 38 龙卷水击：最高HP -10 点 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(10191)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP], 3990,
                 "T2 战斗时最高HP 降低 10 点（削减同步压当前体力、下限钳1）");
        CHECK(ctx.getPet(1).hp < 4000, "T2 当前体力同步下压");
        std::cout << "  [T2] effect 38 通过\n";
    }

    //========== T3 effect 55 属性反转：战斗时属性视图对调 ==========
    {
        // 守方元素 {1,1}（1 vs 1 = 免疫）。反转后攻方视图变 {1,1}、守方变 {0,0}。
        ElfPet foe = foe_pet();
        foe.elementalAttributes[0] = 1;
        foe.elementalAttributes[1] = 1;
        ScenarioBattle b(scenario::make_party(attacker(20113)),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);   // 属性反转（属性技，无红伤）
        CHECK_EQ(ctx.ws.view_elementalAttributes[0][0], 1,
                 "T3 反转：攻方视图换成守方元素（立即可见，本回合伤害读取源）");
        CHECK_EQ(ctx.ws.view_elementalAttributes[1][0], 0,
                 "T3 反转：守方视图换成攻方元素");
        std::cout << "  [T3] effect 55 通过\n";
    }

    //========== T4 effect 63 相位光波：能力下降反馈给对手 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(10859)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        REQUIRE(stat_drop(&ctx, 0, /*速度=*/4, 1, 1) == StatDropResult::SUCCESS);
        use_skill0(b);
        CHECK_EQ(ctx.ability_level(1, 0), -2, "T4 反馈：镜像下降落到对手（攻击-2）");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T4 反馈：自身攻击下降解除");
        CHECK_EQ(ctx.ability_level(0, 4), 0, "T4 反馈：自身速度下降解除");
        std::cout << "  [T4] effect 63 通过\n";
    }

    //========== T5 effect 64 铁山靠：烧伤/冻伤/中毒 → 伤害加倍 ==========
    {
        // 对照：无异常 → 无通用增伤
        {
            ScenarioBattle b(scenario::make_party(attacker(10754)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_skill0(b);
            CHECK_EQ(ctx.ws.damage_add_pct[0], 0, "T5 对照：无状态不加伤");
        }
        // 烧伤 → +100%
        {
            ScenarioBattle b(scenario::make_party(attacker(10754)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(apply_anomaly(&ctx, 0, kBurn, 2, 1) == ApplyAnomalyResult::SUCCESS);
            use_skill0(b);
            CHECK_EQ(ctx.ws.damage_add_pct[0], 100, "T5 烧伤中：伤害反而加倍（加算+100%）");
        }
        std::cout << "  [T5] effect 64 通过\n";
    }

    //========== T6 effect 66 宝石脉冲：击败 → 回最大体力 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(10736)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 1;      // 残血对手（max 4000 / cur 1）
        ctx.getPet(0).hp = 1200;   // 预留回血空间（回满会 clamp，别贴上限）
        use_skill0(b);
        CHECK(ctx.getPet(1).hp <= 0, "T6 前提：对手被击败");
        CHECK_EQ(ctx.getPet(0).hp, 1200 + 1000, "T6 击败对手：恢复自身最大体力 1/3");
        std::cout << "  [T6] effect 66 通过\n";
    }

    //========== T7 effect 67 疲乏撕咬：击败 → 对方下只出战精灵 maxHP -1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(10778)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 1;      // 残血对手（max 4000 / cur 1）
        use_skill0(b);
        CHECK(ctx.getPet(1).hp <= 0, "T7 前提：对手在场宠被击败");
        // 死后补位：守方选下一只出战
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP], 4000 - 4000 / 3,
                 "T7 下只出战精灵：最大体力被削 1/3");
        std::cout << "  [T7] effect 67 通过\n";
    }

    //========== T8 effect 89 蜜桃甜甜弹：每次造成伤害 1/8 回血 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(12124)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2900;   // 3000-100
        use_skill0(b, /*foe_slot=*/0);   // 守方用锋利（属性技）：不回打，隔离回血观测
        const int dealt = 4000 - ctx.getPet(1).hp;
        CHECK(dealt > 0, "T8 前提：打出红伤");
        CHECK_EQ(ctx.getPet(0).hp, 2900 + dealt / 8, "T8 造成伤害的 1/8 恢复自己HP");
        std::cout << "  [T8] effect 89 通过（dealt=" << dealt << "）\n";
    }

    //========== T9 effect 94 石化之视：100% 令对手石化 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(20731)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK(ctx.has_active_abnormal_status(1, kPetrify), "T9 100% 石化落地");
        std::cout << "  [T9] effect 94 通过\n";
    }

    //========== T10 effect 102 电能引爆：对手麻痹 → 威力翻倍 ==========
    {
        // 对照：无麻痹 → 威力视图保持 80
        {
            ScenarioBattle b(scenario::make_party(attacker(11736)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_skill0(b);
            CHECK_EQ(ctx.ws.skill_power_view[0], 80, "T10 对照：无麻痹威力不变");
        }
        // 麻痹 → 160
        {
            ScenarioBattle b(scenario::make_party(attacker(11736)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(apply_anomaly(&ctx, 1, kParalysis, 2, 0)
                    == ApplyAnomalyResult::SUCCESS);
            use_skill0(b);
            CHECK_EQ(ctx.ws.skill_power_view[0], 160, "T10 对手麻痹：威力视图翻倍（变威力）");
        }
        std::cout << "  [T10] effect 102 通过\n";
    }

    //========== T11 effect 106 光涌之爪：特殊攻击对自身必定 miss ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(16528)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);   // 攻方先手：光涌之爪（效果立即生效）→ 守方用槽1 吸取（特殊攻击）
        CHECK_EQ(ctx.ws.hit_rate_mod[1], 0.0f, "T11 攻方 hit_rate_mod 归零");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T11 守方特殊攻击必定 miss（自身不掉血）");
        std::cout << "  [T11] effect 106 通过\n";
    }

    //========== T12 effect 128 异界之光：受到的攻击伤害转化为自身体力 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(21174)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2800;   // 预留净增空间（贴 max 会 clamp）
        use_skill0(b);   // 攻方先手挂转化窗口 → 守方拍打打过来
        const int taken = ctx.ws.raw_attack_damage[1];
        CHECK(taken > 0, "T12 前提：守方打出攻击伤害");
        // 净增口径：受击落地后回 2×落伤 = 净增该伤害值（文件头口径注；官方若为净零改系数）
        CHECK_EQ(ctx.getPet(0).hp, 2800 + taken, "T12 伤害转化为体力（净增）");
        std::cout << "  [T12] effect 128 通过（taken=" << taken << "）\n";
    }

    //========== T13 effect 155 打盹：回满 + 清下降 + 自身睡眠 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(21422)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2500;
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        use_skill0(b, /*foe_slot=*/0);   // 守方用锋利：不回打，隔离回满断言
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T13 恢复全部体力");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T13 消除所有能力下降");
        CHECK(ctx.has_active_abnormal_status(0, kSleep), "T13 自身进入睡眠（2回合）");
        std::cout << "  [T13] effect 155 通过\n";
    }

    //========== T14 effect 165 光影冲击：每回合防御/特防 +1 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(24781)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK_EQ(ctx.ability_level(0, 2), 1, "T14 R1：防御 +1（首手立即 1 次）");
        CHECK_EQ(ctx.ability_level(0, 3), 1, "T14 R1：特防 +1");
        // R2：窗口 tick 再 +1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ability_level(0, 2), 2, "T14 R2：窗口 tick 防御再 +1");
        CHECK_EQ(ctx.ability_level(0, 3), 2, "T14 R2：窗口 tick 特防再 +1");
        std::cout << "  [T14] effect 165 通过\n";
    }

    //========== T15 effect 185 火云狂战破：击败烧伤的对手 → 下只也烧伤 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(15179)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 1;      // 残血对手（max 4000 / cur 1）
        REQUIRE(apply_anomaly(&ctx, 1, kBurn, 5, 0) == ApplyAnomalyResult::SUCCESS);
        use_skill0(b);
        CHECK(ctx.getPet(1).hp <= 0, "T15 前提：烧伤的对手被击败");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T15 下只出战的对手也进入烧伤（异常码 {0} 两侧同值）");
        std::cout << "  [T15] effect 185 通过\n";
    }

    //========== T16 effect 198 八卦断念斩：随机 1 种能力 -1 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(16475)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK_EQ(ability_sum(ctx, 1), -1, "T16 随机 1 项 -1（总量断言，不依赖具体槽）");
        std::cout << "  [T16] effect 198 通过\n";
    }

    //========== T17 effect 440 螺旋爆裂拳：对手 PP 消耗变 3 倍 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(15591)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp_before = ctx.getPet(1).skills[0].pp;
        use_skill0(b, /*foe_slot=*/0);   // 守方用槽0（拍打）：AFTER_ACTION 消耗时按 3 倍扣
        CHECK_EQ(ctx.ws.skill_pp_cost_multiplier[1], 3, "T17 消耗倍数已写入 ws");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, pp_before - 3, "T17 本次消耗 3 点 PP");
        std::cout << "  [T17] effect 440 通过\n";
    }

    //========== T18 effect 599 撼地断岩拳：受特殊攻击伤害 -25% ==========
    {
        // 对照：无窗口时守方吸取对攻方造成的**实际落血**（裸伤台账不含管线减伤，不可用）
        int plain_drop = 0;
        {
            ScenarioBattle b(scenario::make_party(attacker(kHit)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            plain_drop = 3000 - ctx.getPet(0).hp;
            CHECK(plain_drop > 0, "T18 对照前提：守方特殊攻击有落血");
        }
        // 载体（类别码 1=特殊、25%）→ 守方吸取落血 ×0.75
        ScenarioBattle b(scenario::make_party(attacker(16384)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);   // 攻方先手：立即写减伤槽 → 守方吸取（特殊）伤害 ×0.75
        CHECK_EQ(ctx.ws.damage_reduce_mul[0][0], 25, "T18 乘算减伤槽写入 25");
        const int reduced_drop = 3000 - ctx.getPet(0).hp;
        CHECK(reduced_drop > 0 && reduced_drop < plain_drop,
              "T18 行为：减伤后落血低于对照（" << reduced_drop << " < " << plain_drop << "）");
        CHECK(std::fabs(static_cast<double>(reduced_drop) / plain_drop - 0.75) <= 0.2,
              "T18 比值 ≈ 0.75（浮动带宽内）");
        std::cout << "  [T18] effect 599 通过（plain=" << plain_drop
                  << " reduced=" << reduced_drop << "）\n";
    }

    //========== T19 effect 620 威慑镇压：致命一击率 +7/16 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(31647)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK(std::abs(ctx.ws.crit_rate_add[0] - 7 * 100.0f / 16.0f) < 0.01f,
              "T19 crit_rate_add = 7/16（加算百分点槽）");
        std::cout << "  [T19] effect 620 通过\n";
    }

    //========== T20 effect 626 对影独酌：随机 2 项能力 +1 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(19569)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill0(b);
        CHECK_EQ(ability_sum(ctx, 0), 2, "T20 随机 2 项 +1（总量断言）");
        std::cout << "  [T20] effect 626 通过\n";
    }

    //========== T21 effect 660[0] 蛮横重锤：消对手提升成功 → 恢复 100% ==========
    {
        // 对照：对手无提升 → 消强失败 → 不回血
        {
            ScenarioBattle b(scenario::make_party(attacker(30938)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2600;
            use_skill0(b, /*foe_slot=*/0);
            CHECK_EQ(ctx.getPet(0).hp, 2600, "T21 对照：无提升可消 → 不恢复");
        }
        // 对手有提升 → 清除 + 回满
        {
            ScenarioBattle b(scenario::make_party(attacker(30938)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)ctx;
            REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
            ctx.getPet(0).hp = 2600;
            // 守方用拍打（不污染对手等级断言）；单步推进到本方 SKILL_EFFECT 之后断言，
            // 再放行整回合——否则守方反击会污染"回满"断言。
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.ctx().debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 2);
            b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
            b.ctx().debug_step_mode = false;
            BattleContext& ctx2 = b.ctx();
            CHECK_EQ(ctx2.ability_level(1, 0), 0, "T21 对手能力提升被消除");
            CHECK_EQ(ctx2.getPet(0).hp, 3000, "T21 消除成功：恢复 100% 体力");
            b.drive();
        }
        std::cout << "  [T21] effect 660 通过\n";
    }

    //========== T22 effect 660[1] 桓侯震天击：解除自身弱化成功 → 恢复 100% ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(16373)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 3, 1) == StatDropResult::SUCCESS);
        ctx.getPet(0).hp = 2600;
        use_skill0(b, /*foe_slot=*/0);
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T22 自身能力下降被解除");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T22 解除成功：恢复 100% 体力");
        std::cout << "  [T22] effect 660 通过\n";
    }

    //========== T23 effect 931 致命无存击：吸强成功 → 对手随机 1 项 PP 归零 ==========
    {
        ScenarioBattle b(scenario::make_party(attacker(19668)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*特攻=*/1, 1) == StatChangeResult::SUCCESS);
        use_skill0(b);
        CHECK_EQ(ctx.ability_level(0, 1), 1, "T23 吸取：特攻提升搬入自身");
        CHECK_EQ(ctx.ability_level(1, 1), 0, "T23 吸取：对手特攻清零");
        int zero_slots = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zero_slots;
            }
        }
        CHECK(zero_slots >= 1, "T23 吸取成功：对手随机 1 项技能 PP 归零");
        std::cout << "  [T23] effect 931 通过\n";
    }

    std::cout << "scenario_150 all passed" << std::endl;
    return 0;
}
