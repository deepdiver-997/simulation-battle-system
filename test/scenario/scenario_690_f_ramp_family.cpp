#include <utils/sbs_rand.h>
// 场景 690：F9 概率/连用递增族 —— effect 782（概率递增）/ 1443（连用威力递增）/
// 482（概率先制）（moves_lib/f_ramp_family.cpp，看板 F9，2026-09-29）
//
// 载体（全库切片自查）：
//   782：深夜梦魇 31765=[50,20,10,100]（纯 [782] 单效果载体；异常码 20=失明 Blind）
//   1443：冰魄噬魂阵 33099=[1220,1443,1249]、args [2,20,80,40] → 1443=[20,80]
//         （1220 对白板无强化对手 no-op；1249=40% 吸血只回自己，不影响对手掉血）
//   482：炫彩五行光 15535=[482]、args [30,2]（30% 概率先制+2）
//
// 断言组：
//   TA 782 递增：8 回合连用 31765 —— uses 计数、申报概率 50→60→…→100（审计槽精确
//       断言，无随机参与）；后 3 回合概率已 100%（roll_percent≥100 必中契约）必落地；
//       总落地数带界。
//   TB 782 分账：782 与非载体技交替 —— 只 782 计数，换技不串账（第 3 次 782 用申报 70）。
//   TC 1443 梯度：6 回合连用 33099 —— 威力视图 160/192/224/256/288/288（SKILL_EFFECT
//       时点 view+view*pct/100 精确整数），连用计数递增，对手掉血=裸伤台账。
//   TD 1443 断连：33099→拍打→33099 —— 第 2 轮 33099 计数归 1、威力视图回 160
//       （effect_9 权威口径：序号跳变断连重置）。
//   TE 482 统计：40 回合 15535（速度 80）对高速白板（速度 200）—— 先制成功回合
//       preemptive_level[0]==2 ⟺ 拿到先手权；成功/失败两侧均出现，总数带界（期望≈12）。
//   TF 482 对照：载体用非 482 技 —— 无先制加成，速度定序对手先手（确定性）。
//
// 确定性：所有载体/拍打 crit_rate=0（harness 清零）、must_hit=1；同系克制 1.0；
// 482 载体固有 priority=0（常识速查坑表：先制对照必须 priority=0 载体，防固有先制残留）。
// 概率断言口径 = F5/F6/F7 先例（固定种子 + 统计带 + 确定性机械量双保险）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kNightmare = 31765;  // 深夜梦魇：物攻 130、effect [782]=[50,20,10,100]
constexpr int kBingPo = 33099;     // 冰魄噬魂阵：特攻 160、1443 切片 [20,80]
constexpr int kWuxing = 15535;     // 炫彩五行光：特攻 150、482 切片 [30,2]
constexpr int kPaDa = 10004;       // 拍打：物攻 40、无效果
constexpr int kBlind = 20;         // 官方异常码 20 = 失明

// plugin_storage 键段镜像（f_ramp_family.cpp 私有键，场景只读审计；99054x~99065x 段）
constexpr int k1443KeyBase = 990544300;       // +owner*3：[+0]=上次序号 [+1]=技能id [+2]=次数
constexpr int k482ArmKeyBase = 990548200;     // +owner：482 watcher 已挂旗标
constexpr int k782ChanceKeyBase = 990548300;  // +owner：本次申报概率（审计槽）
constexpr int k782UsesKeyBase = 990578200;    // +owner*40000 + skill_id%40000：已用次数

int storage_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

ElfPet nightmare_carrier(int speed) {
    ElfPet p = scenario::make_pet(1, "梦魇", {kNightmare, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

ElfPet bingpo_carrier(int speed) {
    ElfPet p = scenario::make_pet(2, "冰魄", {kBingPo, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

ElfPet wuxing_carrier(int speed) {
    ElfPet p = scenario::make_pet(3, "五行", {kWuxing, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(4, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, slot1 >= 0 ? BattleFsm::ActionType::SELECT_SKILL
                                     : BattleFsm::ActionType::NONE,
                       slot1 >= 0 ? slot1 : 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 782 递增：uses 计数 + 申报概率 50→…→100 + 落地统计 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(nightmare_carrier(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0, "TA 前提：载体固有先制 0");
        constexpr int kUsesKey = k782UsesKeyBase + 0 * 40000 + kNightmare % 40000;
        int landed = 0;
        for (int r = 1; r <= 8; ++r) {
            const int rc = ctx.roundCount;   // 本回合即将模拟（回合完成时才 ++，0 基）
            drive_round(b, 0, -1);   // 对手 NONE：无干扰出手
            const int uses = storage_int(ctx, kUsesKey);
            const int chance = storage_int(ctx, k782ChanceKeyBase);
            CHECK_EQ(uses, r, "TA 核心：uses 计数每用 +1（第 " << r << " 回合）");
            CHECK_EQ(chance, std::min(50 + (r - 1) * 10, 100),
                     "TA 核心：第 " << r << " 用申报概率 = min(50+(n-1)*10, 100)");
            // 落地判定：异常时长 2（random_anomaly_duration 常量，end=roundCount+duration）
            // → 本回合落地 ⟺ end_round == rc+2（上一回合残留的 end = rc+1，可分辨；
            // 670 T2 同款读法）。
            const bool landed_now = ctx.abnormal_status_end_round[1][kBlind] == rc + 2;
            if (landed_now) {
                ++landed;
                CHECK(ctx.has_active_abnormal_status(1, kBlind),
                      "TA 附加：落地回合对手处于失明");
            }
            if (r >= 6) {
                CHECK(landed_now, "TA 核心：第 " << r
                      << " 用申报概率已达 100 → 必落地（递增到顶的可观测面）");
            }
        }
        CHECK(landed >= 3 && landed <= 8,
              "TA 核心：8 回合落地数带界（期望 6.5：50/60/70/80/90/100/100/100），实际 "
                  << landed);
        std::cout << "  [TA] 782 递增（计数/申报概率/落地带）通过（落地 " << landed
                  << "/8）\n";
    }

    //========== TB 782 分账：782 与非载体技交替，计数不串账 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(nightmare_carrier(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        constexpr int kUsesKey = k782UsesKeyBase + 0 * 40000 + kNightmare % 40000;
        drive_round(b, 0, -1);   // 782 第 1 用
        drive_round(b, 0, -1);   // 782 第 2 用
        CHECK_EQ(storage_int(ctx, kUsesKey), 2, "TB 前置：两次 782 → 计数 2");
        drive_round(b, 1, -1);   // 拍打（无 782）：不计数
        CHECK_EQ(storage_int(ctx, kUsesKey), 2, "TB 核心：非载体技不串账（计数仍 2）");
        drive_round(b, 0, -1);   // 782 第 3 用
        CHECK_EQ(storage_int(ctx, kUsesKey), 3, "TB 核心：第 3 次 782 计数 3");
        CHECK_EQ(storage_int(ctx, k782ChanceKeyBase), 50 + (3 - 1) * 10,
                 "TB 核心：第 3 次 782 申报概率 70（分账后的正确梯度）");
        std::cout << "  [TB] 782 owner×技能分账 通过\n";
    }

    //========== TC 1443 梯度：威力视图 160/192/224/256/288/288 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(bingpo_carrier(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0, "TC 前提：载体固有先制 0");
        for (int r = 1; r <= 6; ++r) {
            const int before = ctx.getPet(1).hp;
            drive_round(b, 0, -1);
            const int pct = std::min((r - 1) * 20, 80);   // 首用无加成，封顶 +80 个百分点
            const int expect_view = 160 + 160 * pct / 100;
            CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), expect_view,
                     "TC 核心：第 " << r << " 连用威力视图 " << expect_view
                                    << "（+" << pct << "%）");
            const int count = storage_int(ctx, k1443KeyBase + 0 * 3 + 2);
            CHECK_EQ(count, r, "TC 核心：连用计数 " << r);
            // 通用增伤/护盾白板：对手掉血 = 裸伤台账（威力视图已进伤害公式）。
            const int raw = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(before - ctx.getPet(1).hp, raw,
                     "TC 核心：第 " << r << " 回合掉血=裸伤（裸 " << raw << "）");
            CHECK_EQ(ctx.ws.raw_attack_damage[1], 0, "TC 前提：对手无干扰出手");
        }
        std::cout << "  [TC] 1443 梯度 160/192/224/256/288/288 通过\n";
    }

    //========== TD 1443 断连：换技序号跳变 → 计数归 1、加成清零 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(bingpo_carrier(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, -1);   // 33099 连用 1
        drive_round(b, 0, -1);   // 33099 连用 2
        CHECK_EQ(storage_int(ctx, k1443KeyBase + 0 * 3 + 2), 2, "TD 前置：两连用计数 2");
        drive_round(b, 1, -1);   // 拍打：1443 handler 不触发，序号跳变
        CHECK_EQ(storage_int(ctx, k1443KeyBase + 0 * 3 + 2), 2, "TD 前置：换技回合计数不动");
        drive_round(b, 0, -1);   // 33099 重开：断连重置
        CHECK_EQ(storage_int(ctx, k1443KeyBase + 0 * 3 + 2), 1,
                 "TD 核心：断连后计数归 1（effect_9 口径：序号跳变即断）");
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 160,
                 "TD 核心：断连后威力视图回基线 160（首用无加成）");
        std::cout << "  [TD] 1443 断连重置 通过\n";
    }

    //========== TE 482 统计：先制+2 ⟺ 拿先手，40 回合带界 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(wuxing_carrier(/*speed=*/80)),
                         scenario::make_party(foe(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0,
                 "TE 前提：载体固有先制 0（防固有先制残留污染对照）");
        CHECK_EQ(storage_int(ctx, k482ArmKeyBase), 1, "TE 前提：482 CARRY watcher 已武装");
        constexpr int kTrials = 40;
        int went_first = 0;
        for (int t = 0; t < kTrials; ++t) {
            drive_round(b, 0, 0);   // 双方都用技：先手权判(MOVE_RIGHT)在伤害前
            const bool first0 = ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
            const int lv0 = ctx.ws.preemptive_level[0];
            const int lv1 = ctx.ws.preemptive_level[1];
            CHECK_EQ(first0, lv0 > lv1,
                     "TE 核心：第 " << t + 1 << " 回合先手权 ⟺ 先制等级更高（lv0="
                                    << lv0 << " lv1=" << lv1 << "）");
            CHECK(lv0 == 0 || lv0 == 2,
                  "TE 核心：482 判定二值（成功 +2 / 失败 0），实际 lv0=" << lv0);
            if (first0) {
                ++went_first;
            }
        }
        CHECK(went_first > 0 && went_first < kTrials,
              "TE 核心：成功/失败两侧均出现（30% 概率门生效），先手 " << went_first
                  << "/" << kTrials);
        CHECK(went_first >= 3 && went_first <= 25,
              "TE 核心：40 回合先手数带界（期望≈12，二项(40,0.3) 双侧 0.6% 尾），实际 "
                  << went_first);
        std::cout << "  [TE] 482 概率先制统计 通过（先手 " << went_first << "/"
                  << kTrials << "）\n";
    }

    //========== TF 482 对照：非载体技无先制加成，速度定序 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(wuxing_carrier(/*speed=*/80)),
                         scenario::make_party(foe(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);   // 载体用拍打（无 482）
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TF 核心：非 482 技不加先制");
        REQUIRE(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2);
        std::cout << "  [TF] 482 非载体对照 通过\n";
    }

    std::cout << "scenario_690 all passed" << std::endl;
    return 0;
}
