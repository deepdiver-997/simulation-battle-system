// 场景 039：圣灵谱尼（经典 5000）8 个专属技能 —— 批 3 验证。
//
// 覆盖的技能 / effect（技能 id：effect 列表）：
//   璨灵圣光 24319  : 525 57 610 694 776
//   圣灵悲魂曲 18006: 695 700
//   神灵之触 31140  : 1217 906
//   圣光吟诵 25678  : 936 1236 693
//   神圣复苏 31141  : 1237 1005
//   光荣之梦 25679  : 1238 677 1239 843（843 在场景 038 验过）
//   神圣启示歌 31142: 1222 499
//
// 验证点：
//   Z1  525  2回合内免疫对手攻击伤害：窗口内 0 伤、窗口外照常受伤
//   Z2  776  下2回合攻击伤害翻倍：ws.damage_add_pct 在 R+1 = 100、窗口过后归零
//   Z3  1238 全属性+1；对手**不为混沌系**时提升等级数翻倍（+1 → +2）
//   Z4  1237 双方 PP 归零 + 强化/弱化/回合类全清（消强查免清、清弱化不查）
//   Z5  1005 恢复满体力 + 造成等量百分比伤害
//   Z6  700  先出手降对手所有 PP；1222 先出手随机 2 个技能 PP 归零
//   Z7  906  递增百分比伤害 15→25→35→45（封顶）
//   Z8  694  下2回合必定先出手；499 后出手 → 下回合所有技能先制+3
//   Z9  610  遇到天敌先制+1（看固有属性克制，非技能属性/视图）
//   Z10 936  100% 令对手疲惫；概率未触发（0%）→ 改封属性技能（行为验证）
//   Z11 1236 受攻击时，**造成伤害前**给攻击方附加其最大体力 1/3 的百分比伤害
//   Z12 1217 消强成功 → 对手下 1 次攻击技能无效；无强可消则不挂
//   Z13 57   每回合使用技能恢复 1/3 最大体力；1239 低体力时回血并造成等量百分比伤害
//   Z14 695  概率封属：0% 不封、100% 封
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kCuiLing = 24319;   // 璨灵圣光（属性）
constexpr int kBeiHunQu = 18006;  // 圣灵悲魂曲（攻击，priority 1）
constexpr int kShenLing = 31140;  // 神灵之触（攻击，priority 3）
constexpr int kYinSong = 25678;   // 圣光吟诵（属性）
constexpr int kFuSu = 31141;      // 神圣复苏（属性，priority 2）
constexpr int kGuangRong = 25679; // 光荣之梦（属性）
constexpr int kQiShiGe = 31142;   // 神圣启示歌（攻击，priority 1）
constexpr int kZhuangJi = 10001;  // 撞击（普通攻击）
constexpr int kGaoSu = 20017;     // 高速移动（属性，自身速度+2）

constexpr int kChaos = 222;       // 混沌系
constexpr int kGrass = 1;         // 草系
constexpr int kWater = 2;         // 水系

// 直接调用某个已注册的 effect（精确定量测量用；避免红伤 ±15% 随机混入）。
void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在插件中注册");
    e.logic(&ctx, e.args);
}

scenario::ScenarioBattle make_battle(int speed0, int speed1, int hp0 = 3000, int hp1 = 3000,
                                     int sk0 = kGaoSu, int sk1 = kZhuangJi) {
    auto p0 = scenario::make_pet(0, "谱尼方", {sk0, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 speed0, /*atk=*/100, hp0);
    auto p1 = scenario::make_pet(1, "对手", {sk1, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 speed1, /*atk=*/100, hp1);
    p1.skills[0].must_hit = true;   // 攻击必中，避免 miss 干扰"是否被封"的判断
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 打完一回合（双方都出招）。
void play(scenario::ScenarioBattle& b, int s0, int s1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

// 推进下一回合并单步越过 target 时点（用于观察该时点写入的 ws 值，随后仍需 drive() 收尾）。
// ⚠️ ws 各字段的 memset 发生在各自时点：damage_add_pct 在 ROUND_START 之后写，
//    guaranteed_first / preemptive_level 在 MOVE_RIGHT 开头被 memset、随后由桶写入。
void advance_past(scenario::ScenarioBattle& b, int s0, int s1, State target) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.step_past(target);
}

void advance_to_round_start(scenario::ScenarioBattle& b, int s0, int s1) {
    advance_past(b, s0, s1, State::BATTLE_ROUND_START);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== Z1 525：2回合内免疫对手的攻击伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle(/*speed0=*/300, /*speed1=*/100);
        b.start();
        BattleContext& ctx = b.ctx();
        const int R = ctx.roundCount;
        call_effect(ctx, 525, {0, 1, /*rounds=*/2, /*pct=*/100});  // 0 先出手 → 窗口 [R, R+2)

        play(b, /*0=*/1, /*1=*/0);   // R：1 攻击 0
        CHECK_EQ(ctx.getPet(0).hp, 3000, "Z1) 窗口内（本回合）：对手攻击伤害应为 0");

        play(b, /*0=*/1, /*1=*/0);   // R+1：仍在窗口内
        CHECK_EQ(ctx.getPet(0).hp, 3000, "Z1) 窗口内（第2回合）：仍免疫");

        play(b, /*0=*/1, /*1=*/0);   // R+2：窗口已过
        CHECK(ctx.getPet(0).hp < 3000, "Z1) 窗口外（第3回合）：应照常受到攻击伤害");
        std::cout << "  [Z1] 525 伤害免疫窗口: R=" << R << " 内两次 0 伤、R+2 后受伤（HP="
                  << ctx.getPet(0).hp << "）\n";
    }

    //========== Z2 776：下2回合自身造成的攻击伤害翻倍 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 776, {0, 1, /*rounds=*/2});

        advance_to_round_start(b, 1, 0);   // 该回合 ROUND_START 由 776 的效果写 ws
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 100,
                 "Z2) 「下N回合」→ R+1 起算，该回合 AMP 增伤应为 100%（翻倍）");
        b.drive();

        advance_to_round_start(b, 1, 0);   // R+2：仍在窗口内
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 100, "Z2) R+2 仍在窗口内");
        b.drive();

        advance_to_round_start(b, 1, 0);   // R+3：窗口已过
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0, "Z2) R+3 窗口已过，增伤归零");
        b.drive();
        std::cout << "  [Z2] 776 伤害翻倍窗口: R+1/R+2 增伤 100%、R+3 归零\n";
    }

    //========== Z3 1238：全属性+N，对手非混沌系时提升等级数翻倍 ==========
    {
        // 对手普通系（0）≠ 混沌 → 翻倍：+1 → +2
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 1238, {0, 1, /*delta=*/1, kChaos});
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2, "Z3) 对手非混沌 → 全属性+1 翻倍为 +2");
        }
        std::cout << "  [Z3] 1238 对手非混沌: 全属性 +2（翻倍）\n";
    }
    {
        // 对手混沌系 → 不翻倍：全属性 +1
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).elementalAttributes[0] = kChaos;
        call_effect(ctx, 1238, {0, 1, /*delta=*/1, kChaos});
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "Z3) 对手为混沌系 → 不翻倍，全属性+1");
        }
        std::cout << "  [Z3] 1238 对手为混沌: 全属性 +1（不翻倍）\n";
    }

    //========== Z4 1237：双方 PP 归零 + 消除双方强化/弱化/回合类 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        // 双方各有强化 + 弱化
        ctx.ability_levels[0][0] = 3;
        ctx.ability_levels[0][1] = -2;
        ctx.ability_levels[1][2] = 4;
        ctx.ability_levels[1][3] = -1;
        // 双方的技能 PP 都掉一点，确认会被归零
        ctx.getPet(0).skills[0].pp = 1;
        ctx.getPet(1).skills[0].pp = 3;

        call_effect(ctx, 1237, {});
        for (int side = 0; side <= 1; ++side) {
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(ctx.ability_levels[side][i], 0,
                         "Z4) 双方能力提升与下降都应被消除");
            }
            for (const Skills& sk : ctx.getPet(side).skills) {
                CHECK_EQ(sk.pp, 0, "Z4) 双方所有技能 PP 应归零");
            }
        }
        std::cout << "  [Z4] 1237 神圣复苏: 双方 6 项等级归零、5 个技能 PP 归零\n";
    }

    //========== Z5 1005：恢复最大体力的 1/1 并造成等量百分比伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1200;   // 自身掉血，便于观察"等量"
        const int missing = 3000 - 1200;
        call_effect(ctx, 1005, {0, 1, /*denom=*/1});
        CHECK_EQ(ctx.getPet(0).hp, 3000, "Z5) 恢复自身最大体力的 1/1 → 回满");
        CHECK_EQ(ctx.getPet(1).hp, 3000 - missing, "Z5) 对手受到等量百分比伤害（= 实际恢复量）");
        std::cout << "  [Z5] 1005: 恢复 " << missing << " → 对手受到等量 " << missing
                  << " 百分比伤害\n";
    }

    //========== Z6 700 / 1222：先出手的 PP 压制 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);   // 0 先出手
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp_before = ctx.getPet(1).skills[0].pp;
        call_effect(ctx, 700, {0, 1, /*amount=*/1});
        CHECK_EQ(ctx.getPet(1).skills[0].pp, pp_before - 1, "Z6) 700 先出手：对手所有技能 PP -1");

        // 对照：后出手不触发
        scenario::ScenarioBattle b2 = make_battle(100, 300);   // 0 后出手
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        ctx2.ws.preemptive_right = PreemptiveRight::SEER_ROBOT_2;   // 1 先出手 → 0 后出手
        const int pp2_before = ctx2.getPet(1).skills[0].pp;
        call_effect(ctx2, 700, {0, 1, 1});
        CHECK_EQ(ctx2.getPet(1).skills[0].pp, pp2_before, "Z6) 700 后出手：不触发");
        std::cout << "  [Z6] 700 先出手降 PP: 先手 -1、后手不变\n";
    }
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ws.preemptive_right = PreemptiveRight::SEER_ROBOT_1;   // 0 先出手
        for (Skills& sk : ctx.getPet(1).skills) {
            sk.pp = 5;
        }
        call_effect(ctx, 1222, {0, 1, /*count=*/2});
        int zeroed = 0;
        for (const Skills& sk : ctx.getPet(1).skills) {
            if (sk.pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 2, "Z6) 1222 先出手：随机恰好 2 个技能 PP 归零");
        std::cout << "  [Z6] 1222 随机 PP 归零: 5 个技能里归零 " << zeroed << " 个\n";
    }

    //========== Z7 906：递增百分比伤害（15% → 25% → 35% → 45% 封顶）==========
    {
        // 血池给足，避免第 4/5 次的伤害被"剩余体力"截断（否则测到的是 clamp 不是档位）
        scenario::ScenarioBattle b = make_battle(300, 100, /*hp0=*/3000, /*hp1=*/1000000);
        b.start();
        BattleContext& ctx = b.ctx();
        std::vector<int> losses;
        for (int i = 0; i < 5; ++i) {
            ctx.getPet(1).hp = 1000000;   // 每次前回满，避免伤害被"剩余体力"截断
            const int before = ctx.getPet(1).hp;
            call_effect(ctx, 906, {0, 1, /*base=*/15, /*step=*/10, /*cap=*/45});
            losses.push_back(before - ctx.getPet(1).hp);
        }
        // 对手最大体力 1000000 → 15%/25%/35%/45%/45% = 150000/250000/350000/450000/450000
        CHECK_EQ(losses[0], 150000, "Z7) 第1次：15%");
        CHECK_EQ(losses[1], 250000, "Z7) 第2次：+10% → 25%");
        CHECK_EQ(losses[2], 350000, "Z7) 第3次：→ 35%");
        CHECK_EQ(losses[3], 450000, "Z7) 第4次：→ 45%（正好到顶）");
        CHECK_EQ(losses[4], 450000, "Z7) 第5次：封顶 45%");
        std::cout << "  [Z7] 906 递增百分比: 15/25/35/45/45%（第4次到顶后不再增加）\n";
    }

    //========== Z8 694 必先窗口 / 499 后出手下回合先制 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 694, {0, 1, /*rounds=*/2});
        advance_past(b, 1, 1, State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK(ctx.ws.guaranteed_first[0] > 0, "Z8) 694「下N回合必定先出手」：R+1 必先生效");
        b.drive();
        std::cout << "  [Z8] 694 必先: R+1 guaranteed_first[0]=" << ctx.ws.guaranteed_first[0] << "\n";
    }
    {
        scenario::ScenarioBattle b = make_battle(100, 300);   // 0 后出手
        b.start();
        BattleContext& ctx = b.ctx();
        // 先推进到先手权结算之后，使 is_second_mover 可用
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK(ctx.is_second_mover(0), "Z8) 前置：0 号应为后出手");
        call_effect(ctx, 499, {0, 1, /*bonus=*/3});
        b.drive();

        advance_past(b, 1, 1, State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK(ctx.ws.preemptive_level[0] >= 3, "Z8) 499 后出手 → 下回合所有技能先制+3");
        b.drive();
        std::cout << "  [Z8] 499 后出手下回合先制: R+1 preemptive_level[0]="
                  << ctx.ws.preemptive_level[0] << "\n";
    }

    //========== Z9 610：遇到天敌先制+1 ==========
    {
        // 我方水(2)、对手草(1)：草克水 → 对手是我方天敌
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).elementalAttributes[0] = kWater;
        ctx.getPet(1).elementalAttributes[0] = kGrass;
        call_effect(ctx, 610, {0, 1, /*bonus=*/1});
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "Z9) 遇天敌 → 先制+1");

        // 对照：同为普通系，无天敌关系 → 不加
        scenario::ScenarioBattle b2 = make_battle(300, 100);
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        call_effect(ctx2, 610, {0, 1, 1});
        CHECK_EQ(ctx2.ws.preemptive_level[0], 0, "Z9) 非天敌 → 不加先制");
        std::cout << "  [Z9] 610 天敌先制: 天敌 +1、非天敌 +0\n";
    }

    //========== Z10 936：概率异常；未触发则封属性 ==========
    {
        // 100% → 必定给对手疲惫
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 936, {0, 1, /*chance=*/100, /*anomaly=*/7 /*疲惫*/, /*rounds=*/2});
        CHECK(ctx.abnormal_status_end_round[1][7] > 0, "Z10) 100% → 对手陷入疲惫");
        std::cout << "  [Z10] 936 100%: 对手疲惫（失效回合="
                  << ctx.abnormal_status_end_round[1][7] << "）\n";
    }
    {
        // 0% → 未触发 → 封对手属性技能 2 回合（行为验证：对手的属性技能打不出效果）
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 936, {0, 1, /*chance=*/0, /*anomaly=*/7, /*rounds=*/2});
        CHECK_EQ(ctx.abnormal_status_end_round[1][7], 0, "Z10) 0% → 不给异常");
        play(b, /*0=*/1, /*1=*/1);   // 1 用高速移动（属性）
        CHECK_EQ(ctx.ability_levels[1][4], 0, "Z10) 未触发 → 封属性，对手属性技能无效");

        // 对照：同样的回合但没被封 → 高速移动生效
        scenario::ScenarioBattle bc = make_battle(300, 100);
        bc.start();
        BattleContext& c2 = bc.ctx();
        play(bc, /*0=*/1, /*1=*/1);
        CHECK(c2.ability_levels[1][4] > 0, "Z10) 对照：未被封属时属性技能正常生效");
        std::cout << "  [Z10] 936 未触发→封属: 被封时速度等级=" << ctx.ability_levels[1][4]
                  << "、对照=" << c2.ability_levels[1][4] << "\n";
    }

    //========== Z11 1236：受攻击时，造成伤害前给攻击方附加其最大体力 1/3 百分比伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle(/*speed0=*/100, /*speed1=*/300,
                                                 /*hp0=*/3000, /*hp1=*/3000);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 1236, {0, 1, /*rounds=*/4, /*denom=*/3});
        const int opp_hp_before = ctx.getPet(1).hp;
        play(b, /*0=*/1, /*1=*/0);   // 1（先出手）攻击 0
        const int opp_lost = opp_hp_before - ctx.getPet(1).hp;
        CHECK_EQ(opp_lost, 1000, "Z11) 对手（攻击方）应受到其最大体力 1/3 = 1000 的百分比伤害");
        CHECK(ctx.getPet(0).hp < 3000, "Z11) 攻击本身的伤害照常结算");
        std::cout << "  [Z11] 1236 受击惩罚: 攻击方掉 " << opp_lost
                  << "（1/3 最大体力），我方掉 " << (3000 - ctx.getPet(0).hp) << "\n";
    }

    //========== Z12 1217：消强成功 → 对手下 1 次攻击技能无效 ==========
    {
        scenario::ScenarioBattle b = make_battle(/*speed0=*/300, /*speed1=*/100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手有强化
        call_effect(ctx, 1217, {0, 1, /*times=*/1});
        CHECK_EQ(ctx.ability_levels[1][0], 0, "Z12) 消强成功");
        play(b, /*0=*/1, /*1=*/0);   // 1 攻击 0 → 应被判为技能无效
        CHECK_EQ(ctx.getPet(0).hp, 3000, "Z12) 消强成功 → 对手下 1 次攻击技能无效（0 伤）");
        play(b, /*0=*/1, /*1=*/0);   // 次数用掉后再攻击 → 恢复伤害
        CHECK(ctx.getPet(0).hp < 3000, "Z12) 封攻击只 1 次，之后照常受伤");

        // 对照：对手本无强化 → 不挂封攻击
        scenario::ScenarioBattle bc = make_battle(300, 100);
        bc.start();
        BattleContext& c2 = bc.ctx();
        call_effect(c2, 1217, {0, 1, 1});
        play(bc, /*0=*/1, /*1=*/0);
        CHECK(c2.getPet(0).hp < 3000, "Z12) 对照：无可消强化 → 不封攻击");
        std::cout << "  [Z12] 1217 消强→封攻击: 封住时 0 伤、用掉后受伤；无强可消不封\n";
    }

    //========== Z13 57 每回合用技能回血 / 1239 低体力回血+等量百分比 ==========
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1500;   // 掉一半血
        call_effect(ctx, 57, {0, 1, /*rounds=*/5, /*denom=*/3});
        play(b, /*0=*/1, /*1=*/1);   // 0 使用技能（高速移动）
        CHECK_EQ(ctx.getPet(0).hp, 2500, "Z13) 57 每回合使用技能恢复最大体力 1/3 = 1000");
        std::cout << "  [Z13] 57 持续回血: 1500 → " << ctx.getPet(0).hp << "（+1000 = 1/3）\n";
    }
    {
        // 677：N回合内每回合使用技能 → 吸取对手能力提升状态
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 677, {0, 1, /*rounds=*/3});
        ctx.ability_levels[1][0] = 2;    // 对手有强化，等被吸
        play(b, /*0=*/1, /*1=*/1);      // 0 使用技能
        CHECK_EQ(ctx.ability_levels[1][0], 0, "Z13) 677 吸取后：对手强化被搬走");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "Z13) 677 吸取后：强化落到自己身上");
        std::cout << "  [Z13] 677 每回合吸取强化: 对手 +2 → 0、自身 0 → "
                  << ctx.ability_levels[0][0] << "\n";
    }
    {
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 低于 1/2（1500）
        call_effect(ctx, 1239, {0, 1, /*denom_heal=*/2, /*denom_threshold=*/2});
        CHECK_EQ(ctx.getPet(0).hp, 2500, "Z13) 1239 恢复最大体力 1/2 = 1500");
        CHECK_EQ(ctx.getPet(1).hp, 3000 - 1500, "Z13) 体力低于 1/2 → 造成等量百分比伤害");

        // 对照：体力高于阈值 → 只回血，不打百分比
        scenario::ScenarioBattle bc = make_battle(300, 100);
        bc.start();
        BattleContext& c2 = bc.ctx();
        c2.getPet(0).hp = 2000;   // 高于 1/2
        call_effect(c2, 1239, {0, 1, 2, 2});
        CHECK_EQ(c2.getPet(0).hp, 3000, "Z13) 对照：仍然回血");
        CHECK_EQ(c2.getPet(1).hp, 3000, "Z13) 对照：体力高于阈值 → 不给百分比伤害");
        std::cout << "  [Z13] 1239 低血附伤: 低血时对手掉 1500、高血时不掉\n";
    }

    //========== Z14 695：概率封属 ==========
    {
        // 100% → 必然封住对手的属性技能
        scenario::ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        call_effect(ctx, 695, {0, 1, /*rounds=*/3, /*chance=*/100});
        play(b, /*0=*/1, /*1=*/1);
        CHECK_EQ(ctx.ability_levels[1][4], 0, "Z14) 100% 封属 → 对手属性技能无效");

        // 0% → 一次都封不住
        scenario::ScenarioBattle b2 = make_battle(300, 100);
        b2.start();
        BattleContext& c2 = b2.ctx();
        call_effect(c2, 695, {0, 1, /*rounds=*/3, /*chance=*/0});
        play(b2, /*0=*/1, /*1=*/1);
        CHECK(c2.ability_levels[1][4] > 0, "Z14) 0% 封属 → 对手属性技能照常生效");
        std::cout << "  [Z14] 695 概率封属: 100% 封住（速度等级 0）、0% 不封（速度等级 "
                  << c2.ability_levels[1][4] << "）\n";
    }

    std::cout << "\n场景 039 通过：圣灵谱尼 8 个专属技能（免伤窗口/伤害翻倍/全属性翻倍/"
                 "双方净化/等量百分比/PP压制/递增百分比/必先与后手先制/天敌先制/概率异常与封属/"
                 "受击惩罚/消强封攻击/持续回血）逐条落地。\n";
    return 0;
}
