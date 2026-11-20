// 场景 670 —— F7 概率持续固伤族集成（603 / 76 / 20）（2026-09-29）
//
// 机制（f_dot_family.cpp）：
//   603 = apply_anomaly_checked 概率申报 + **指定时长**{2}（异常通道，全检查链）；
//   76  = roll_percent 概率门 → InRounds 窗挂回合扣减点（ALL_ROUND_MINUS），每回合
//         deal_pink_damage(FIXED, {2})（独立 dot，非异常）；
//   20  = 自伤技：对自己申报疲惫(7)（时长 {1}+1 修正）——疲惫 = 控制类异常，
//         has_blocking_control_status 在 ACTION_START 整段禁主流程（引擎语义免加通道）。
//
// 断言组：
//   T1 603 @ 异元幻空斩 16349（[195,603]|[100,16,2]，**100% 切片**）：命中 → 对手流血(16)
//      落地、end_round = R+2（指定时长，非随机）；R2 对手行动开始被扣恰 80（流血固定档）；
//      R3 解除（0 扣血）——时长精确 2。
//   T2 603 @ 飞沙诀 16385（[603]|[25,16,1]，25% 切片）：40 回合循环统计，成功/失败两侧
//      均出现（概率闸门真实掷点；全败 p≈0.75^40≈1e-5，全胜 p≈0——E13 场景 590 同款侧证法）。
//   T3 76 @ 痛苦撕裂 11143（[76]|[100,5,100]，**100% 切片**）：R1（当回合末）起每回合
//      扣减点恰 100 点 FIXED ×5（R1..R5），R6 归零——"N回合内"窗口恰 N 次结算。
//   T4 20 @ 火箭头槌 10072（[20]|[100,1]，**100% 切片**）：命中 → 自己疲惫 end=R+2；
//      R2 行动被封（对手 0 掉血）；R3 解封恢复出手——自伤 +1 修正 + "无法攻击"引擎语义。
//
// 场景坑对表：威力 0 的 11143 有 1 点保底伤（T3 的 R1 攻击分量按 1~2 容忍）；双方 NONE
// filler 出手；异常 dot 在 ACTION_START 结算、与 skip 决策无关（battleFsm stage→settle 前置）。
// ⚠️ roundCount 口径：**回合进行中 0 基**（advanceRound 在 Round Completion 才走）——
// R1 内施加 duration=d → end = 0+d，active 覆盖日历回合 1..d；drive() 返回时本回合已完成、
// roundCount 已 +1 → "驱动后仍 active"类断言不可写，用 end_round 绝对值与逐回合掉血表达。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYiyuan = 16349;    // 异元幻空斩 [195,603]|[100,16,2]（100% 流血2回合）
constexpr int kFeisha = 16385;    // 飞沙诀 [603]|[25,16,1]（25% 流血1回合）
constexpr int kTongku = 11143;    // 痛苦撕裂 [76]|[100,5,100]（100% 5回合×100）
constexpr int kHuojian = 10072;   // 火箭头槌 [20]|[100,1]（100% 自疲惫1回合）
constexpr int kZhuangJi = 10001;  // 撞击（filler 攻击）
constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);      // 16
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 7

ElfPet make_attacker(int skill, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "攻方", {skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe(int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, /*hp=*/1000000);
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

    // ════════════════════════════════════════════════════════════════
    // T1 603 @16349（100%）：对手流血、指定时长 2 → end=R+2；R2 扣 80、R3 解除
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(kYiyuan)), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 异元幻空斩
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T1: 100% 切片 → 对手流血(16)落地");
        CHECK_EQ(ctx.abnormal_status_end_round[1][kBleed], 2,
                 "T1: 指定时长 {2}=2 → end=0+2（回合中 roundCount 0 基；覆盖日历 R1..R2，非随机时长）");
        int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 双方都不出手
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 80,
                 "T1: 流血第 2 次结算恰 80（R1 行动开始已结算过第 1 次）");
        hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R3 双方都不出手
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 0, "T1: R3 流血已到期（0 扣血）——时长精确 2");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T1: R3 流血已解除");
        std::cout << "  [T1] 603 @16349 流血2回合 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 603 @16385（25%）：40 回合循环统计——成功/失败两侧均出现
    // ════════════════════════════════════════════════════════════════
    {
        ElfPet att = make_attacker(kFeisha);
        for (Skills& s : att.skills) {
            s.maxPP = 100;   // 40 回合连发（原 pp=20 不够）
            s.pp = 100;
        }
        ScenarioBattle b(make_party(att), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        constexpr int kTrials = 40;
        int landed = 0;
        for (int t = 0; t < kTrials; ++t) {
            const int r = ctx.roundCount;   // 本回合即将模拟
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 16385 的 {2}=1 → "本回合落地" ⟺ end_round == r+1（前回合残留 end ≤ r，可分辨）
            if (ctx.abnormal_status_end_round[1][kBleed] == r + 1) {
                ++landed;
            }
        }
        CHECK(landed > 0, "T2: 25% 概率门 40 回合全失败（p≈1e-5，视为闸门坏）");
        CHECK(landed < kTrials, "T2: 25% 概率门 40 回合全成功（概率未生效）");
        std::cout << "  [T2] 603 @16385 25% 循环统计 通过 (落地 " << landed << "/" << kTrials
                  << "，期望≈10)\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 76 @11143（100%）：R1..R5 每回合扣减点恰 -100，R6 归零
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(kTongku)), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 痛苦撕裂（威力0→保底1）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int drop_r1 = 1000000 - ctx.getPet(1).hp;
        CHECK(drop_r1 >= 101 && drop_r1 <= 102,
              "T3: R1 = dot 首次结算 100 + 攻击保底分量 1~2（当回合末即结算，InRounds 口径）");
        for (int r = 2; r <= 5; ++r) {
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(hp_before - ctx.getPet(1).hp, 100,
                     "T3: dot 第 r-1 次结算恰 100（FIXED 粉伤，无浮动）");
        }
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R6：窗口已过期
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 0, "T3: R6 窗口到期（{1}=5 次结算后归零）");
        std::cout << "  [T3] 76 @11143 5回合×100 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 20 @10072（100%）：自疲惫 end=R+2；R2 封行动、R3 解封
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(kHuojian, /*speed=*/300, /*atk=*/200)),
                         make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 火箭头槌
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(0, kFatigue), "T4: 100% → 自己疲惫(7)落地（自伤技）");
        CHECK_EQ(ctx.abnormal_status_end_round[0][kFatigue], 2,
                 "T4: 自伤 +1 修正 → end=0+2（覆盖日历 R1..R2，封 R2 一次出手）");
        const int foe_hp_after_r1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 想出手——被疲惫封
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_after_r1,
                 "T4: 疲惫回合行动被封（has_blocking 控制类禁行动，对手 0 掉血）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 解封恢复出手
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(0, kFatigue), "T4: R3 疲惫已解除");
        CHECK(ctx.getPet(1).hp < foe_hp_after_r1, "T4: R3 解封恢复出手（对手掉血）");
        std::cout << "  [T4] 20 @10072 自疲惫封行动 通过\n";
    }

    std::cout << "scenario_670 PASS (F7 概率持续固伤族 603/76/20)" << std::endl;
    return 0;
}
