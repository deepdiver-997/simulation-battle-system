#include <utils/sbs_rand.h>
// 场景 2090 —— J115 条件先制族二（4 宏，moves_lib/j_cond_prio2.cpp）
//（2026-10-03，worktree ../agents/j115；号段 2090-2099 → T2090-T2093 四块）
//
// 断言口径：
//   - 958（鬼幽斩杀 [6,1] 物130 纯链 prio0）：对手预置害怕(6) → R1 选择期先制+1 翻盘
//     （self 100 vs 300——**首回合当次生效**，这正是选择期路由对 case 路由的判据性差异，
//     J66 勘误）；负例×2 = 无异常 / 异常码不符（预置中毒 1）→ 对手先手。
//   - 1174（无光之火 特130 纯链 零参）：自身预置 +2 槽 → R1 翻盘；对照 = 无提升 →
//     对手先手。
//   - 1081（铁之幻光 特130 纯链 零参）：对手预置 +2 槽 → R1 翻盘；对照 = 无提升 →
//     对手先手。
//   - 975（能源轰炸 [15,3,2] 特130 纯链，p=0.15 统计侧证，J107 1003 同风格）：100 局，
//     每局 R1 self 用载体（慢速）记对手掉血 L1，R2 双方再出手看先手方——
//     · 概率带：成功局（R2 对手先手）∈ [3,30]（二项 B(100,0.15) 双侧尾 <0.3%）；
//     · 伤害半：成功局 L1 最小值 > 2×失败局最大值（×3 带 2.55~3.0 对 ×1 带 0.85~1.0，
//       阈值 2× 居中）；R2 先手方即 roll 分支的观测面（fail → NextRounds 先制+2 翻盘）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 基线
constexpr int kGuiYou  = 32422;  // 鬼幽斩杀 [958]|[6,1]：物130（958 载体，prio0）
constexpr int kWuGuang = 32915;  // 无光之火 [1174]：特130（1174 载体，prio0）
constexpr int kTieZhi  = 33512;  // 铁之幻光 [1081]：特130（1081 载体，prio0）
constexpr int kNengYuan= 30565;  // 能源轰炸 [975]|[15,3,2]：特130（975 载体，prio0）
constexpr int kMaxHp   = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/300, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/300, 99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261003);

    // ══ T2090 958 害怕门先制+1（鬼幽斩杀 [6,1]）══
    {
        // a) 对手害怕 → R1 翻盘先手
        {
            ScenarioBattle b(
                make_party(mk_self({kGuiYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, /*害怕=*/6, /*duration=*/5, /*actor=*/0);
            CHECK(ctx.has_active_abnormal_status(1, 6), "T2090.a 前置：对手已害怕");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T2090.a 958：对手害怕 → 首回合先制+1 翻盘先手");
        }
        // b) 负例：无异常 → 门关 → 对手先手
        {
            ScenarioBattle b(
                make_party(mk_self({kGuiYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T2090.b 958 对照：无异常 → 对手先手");
        }
        // c) 负例：异常码不符（中毒 1 ≠ 害怕 6）→ 对手先手
        {
            ScenarioBattle b(
                make_party(mk_self({kGuiYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, /*中毒=*/1, /*duration=*/5, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T2090.c 958 对照：码不符（中毒）→ 对手先手");
        }
        std::cout << "  [T2090] 958 害怕门先制 通过\n";
    }

    // ══ T2091 1174 自身提升门先制+2（无光之火 零参）══
    {
        // a) 自身 +2 槽 → R1 翻盘先手
        {
            ScenarioBattle b(
                make_party(mk_self({kWuGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);   // 自身攻击 +2
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T2091.a 1174：自身有提升 → 首回合先制+2 翻盘先手");
        }
        // b) 对照：无提升 → 对手先手
        {
            ScenarioBattle b(
                make_party(mk_self({kWuGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T2091.b 1174 对照：无提升 → 对手先手");
        }
        std::cout << "  [T2091] 1174 自身提升门先制 通过\n";
    }

    // ══ T2092 1081 对手提升门先制+1（铁之幻光 零参）══
    {
        // a) 对手 +2 槽 → R1 翻盘先手
        {
            ScenarioBattle b(
                make_party(mk_self({kTieZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T2092.a 1081：对手有提升 → 首回合先制+1 翻盘先手");
        }
        // b) 对照：无提升 → 对手先手
        {
            ScenarioBattle b(
                make_party(mk_self({kTieZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T2092.b 1081 对照：无提升 → 对手先手");
        }
        std::cout << "  [T2092] 1081 对手提升门先制 通过\n";
    }

    // ══ T2093 975 概率威力×3 / 未触发下回合先制+2（能源轰炸 [15,3,2]，统计侧证）══
    {
        constexpr int kTrials = 100;
        int succ = 0;                       // R2 对手先手 = R1 roll 成功（无先制加持）
        std::vector<int> loss_succ, loss_fail;
        loss_succ.reserve(kTrials);
        loss_fail.reserve(kTrials);
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(
                make_party(mk_self({kNengYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss1 = hp0 - ctx.getPet(1).hp;
            CHECK(loss1 > 0, "T2093 前置：R1 载体应造成伤害");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 载体
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2) {
                ++succ;   // R1 roll 成功 → 无先制窗 → 速度定序对手先手
                loss_succ.push_back(loss1);
            } else {
                CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                      "T2093 R2 先手方应为双方之一");
                loss_fail.push_back(loss1);   // R1 roll 失败 → 先制+2 翻盘
            }
        }
        CHECK(succ >= 3 && succ <= 30,
              "T2093 975 概率带：100 局成功数 " + std::to_string(succ) +
                  " 应落在 B(100,0.15) 双侧尾 <0.3% 的 [3,30]");
        int mn_succ = kMaxHp, mx_fail = 0;
        for (int v : loss_succ) mn_succ = std::min(mn_succ, v);
        for (int v : loss_fail) mx_fail = std::max(mx_fail, v);
        CHECK(mn_succ > 2 * mx_fail,
              "T2093 975 伤害半：成功局最小掉血 " + std::to_string(mn_succ) +
                  " 应 > 2×失败局最大掉血 " + std::to_string(mx_fail) + "（×3 带对 ×1 带）");
        std::cout << "  [T2093] 975 统计侧证 通过（成功 " << succ << "/" << kTrials
                  << "，伤害带 " << mn_succ << " vs " << mx_fail << "）\n";
    }

    std::cout << "scenario_2090 PASS (J115 条件先制族二 958/975/1081/1174)" << std::endl;
    return 0;
}
