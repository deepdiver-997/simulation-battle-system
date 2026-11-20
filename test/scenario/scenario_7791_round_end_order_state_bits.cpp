// 场景 7791 —— 回合时点两笔 S18 清缴实证（09 章待验证清单 #1/#2）：
//
//   R 组 ★ BATTLE_ROUND_END 定序 =「先手权方后结算」（09 章 §八#1 的引擎半边）：
//     SEER_ROBOT_1 = 房主(0)侧持先手权、SEER_ROBOT_2 = 挑战(1)侧
//     （battleFsm.cpp resolve_first_mover_id + battleContext.h is_second_mover
//     双源钉死）。handler 分支（battleFsm.cpp:2126-2138）：
//       ROBOT_1 → execute(1)→execute(0)；ROBOT_2 → execute(0)→execute(1)
//       ⇒ 持权方**总是后结算**。
//     官方两句（时点判定流程表 §回合结束时）：
//       ①「双方都不出手时，获得先手权的一方先结算」；
//       ②「双方均出手或者任意一方出手时，未获得先手权的一方先结算」。
//     ⇒ R1（均出手）/R2（一方嗑药）引擎=持权方后结算，与官方句②一致；
//       R3（双方嗑药）引擎先手权由 sbs_rand tie-break **随机**判定（battleFsm.cpp
//       :1764-1768）且仍后结算 —— 与官方句①相悖（官方-引擎分歧实录，内核轮待修）。
//     R3 无随机断言：白盒读 ctx.preemptive_right 后断言不变量
//     「账本序 = [非持权方, 持权方]」，对随机两种结果都精确成立。
//
//   C 组 ★ state.h 覆盖位图头注勘误的用例（09 章 §八#2）：枚举 -1..40 = 42 项、
//     归一化 bit = int+1 落 0..41 —— FINISHED=bit41、ROUND_COMPLETION=bit40
//     （state.h:59-61 头注「41 个状态（-1..39）落低 40 位」为过期注释，内核不改）。
//     static_assert 钉死两位偏移 + coverage_all 对 bit40/41 的运行时查询用例。
//
// 探针：registerEffect(ROUND_END, owner, ContinuousEffect, Skill) —— 双侧各一条，
//   回合结束时把 owner 追加进账本；ROUND_END 非动作流程时点，嗑药轮照跑
//   （is_action_flow_state 外，execute_registered_actions 不守门）。
// 断言纪律：R1/R2 无随机（速度/嗑药分支确定）；R3 不变量对随机精确；
//   全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
#include <cstdint>
#include <iostream>
#include <vector>

#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kHit = 10004;    // 拍打（物理攻击，纯链载体）
constexpr int kHp = 5000;

// ROUND_END 探针：把本桶 owner 追加进账本（全局序）。
std::vector<int> g_re_ledger;

EffectResult round_end_probe(BattleContext*, const EffectArgs& args) {
    if (args.int_count >= 1) {
        g_re_ledger.push_back(args.int_args[0]);
    }
    return EffectResult::kOk;
}

void install_round_end_probe(BattleContext& ctx, int owner) {
    Effect probe_eff(/*id=*/979101, /*priority=*/0, /*owner=*/owner, /*left_round=*/0,
                     EffectArgs(std::vector<int>{owner}), &round_end_probe);
    // ⚠️ duration 必须 = -1（常驻）：duration=0 的条目 isExpired 恒真（currentRound -
    //    registered_round >= 0），注册即过期、永不执行（continuousEffect.h:77-82）。
    auto ce = std::make_unique<ContinuousEffect>(
        probe_eff, State::BATTLE_ROUND_END, /*owner=*/owner, /*duration=*/-1,
        ctx.roundCount);
    ctx.registerEffect(State::BATTLE_ROUND_END, owner, std::move(ce),
                       EffectContainer::Skill);
}

scenario::ScenarioBattle make_battle(const MedicineStock& meds0 = {},
                                     const MedicineStock& meds1 = {},
                                     int speed0 = 200, int speed1 = 100) {
    auto p0 = scenario::make_pet(0, "房主", {kHit, kHit, kHit, kHit, kHit},
                                 speed0, /*atk=*/100, kHp);
    auto p1 = scenario::make_pet(1, "挑战", {kHit, kHit, kHit, kHit, kHit},
                                 speed1, /*atk=*/100, kHp);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1),
                                    meds0, meds1);
}

void reset_ledger() { g_re_ledger.clear(); }

}  // namespace

// C 组：位偏移编译期钉死（state_coverage_bit 的归一化公式 = (int)State + 1，
// state.h:62-64；加/删枚举项即编译红——正是头注勘误要锁的形状）。
static_assert(static_cast<int>(State::GAME_START) + 1 == 0, "GAME_START(-1) → bit0");
static_assert(static_cast<int>(State::BATTLE_ROUND_COMPLETION) + 1 == 40,
              "ROUND_COMPLETION(39) → bit40");
static_assert(static_cast<int>(State::FINISHED) + 1 == 41,
              "FINISHED(40) → bit41（头注「低 40 位」过期：实况 42 项落 0..41）");

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== R1 ★ 均出手（速度定权）：持权方(0)后结算 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        install_round_end_probe(b.ctx(), 0);
        install_round_end_probe(b.ctx(), 1);
        reset_ledger();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 跑完整回合（到下一 CHOOSE 停）
        CHECK_EQ(g_re_ledger.size(), 2u, "R1) 双方各跑一条 ROUND_END 探针");
        CHECK_EQ(b.ctx().preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "R1) 速度 200>100 → SEER_ROBOT_1（房主 0 侧持先手权）");
        CHECK_EQ(b.ctx().prev_round_preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "R1) RC 桶先手权快照同步（旁证回合确实走完）");
        CHECK_EQ(g_re_ledger[0], 1, "R1) ★ 先跑的是 1 侧（未持权方）");
        CHECK_EQ(g_re_ledger[1], 0, "R1) ★ 持权方(0)后结算 —— 与官方句②一致");
        std::cout << "  [R1] 均出手：权=ROBOT_1(0侧) → 账本 [1,0]（持权方后）\n";
    }

    //========== R2 ★ 一方嗑药：官方句②「任意一方出手」分支，持权方(1)后结算 ==========
    {
        const MedicineStock meds0{{300011, 5}};   // 房主嗑初级体力药剂
        scenario::ScenarioBattle b = make_battle(meds0);
        b.start();
        install_round_end_probe(b.ctx(), 0);
        install_round_end_probe(b.ctx(), 1);
        reset_ledger();
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "R2) 0 嗑药 → SEER_ROBOT_2（挑战 1 侧持先手权，无随机）");
        CHECK_EQ(g_re_ledger[0], 0, "R2) ★ 先跑 0 侧（未持权方）");
        CHECK_EQ(g_re_ledger[1], 1, "R2) ★ 持权方(1)后结算 —— 官方句②「任意一方出手」");
        std::cout << "  [R2] 一方嗑药：权=ROBOT_2(1侧) → 账本 [0,1]（持权方后）\n";
    }

    //========== R3 ★ 双方嗑药：权随机 tie-break 但「持权方后结算」不变量精确成立 ==========
    {
        const MedicineStock meds0{{300011, 5}};
        const MedicineStock meds1{{300011, 5}};
        scenario::ScenarioBattle b = make_battle(meds0, meds1);
        b.start();
        install_round_end_probe(b.ctx(), 0);
        install_round_end_probe(b.ctx(), 1);
        reset_ledger();
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(b.ctx().preemptive_right != PreemptiveRight::NONE,
              "R3) 双嗑药轮先手权被 sbs_rand tie-break 判定（NONE 不残留）");
        const int holder = b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1 ? 0 : 1;
        CHECK_EQ(g_re_ledger.size(), 2u, "R3) 双方探针各跑一条");
        CHECK_EQ(g_re_ledger[0], 1 - holder,
                 "R3) ★ 账本首 = 非持权方（无论随机判给谁）");
        CHECK_EQ(g_re_ledger[1], holder,
                 "R3) ★ 持权方后结算 —— 引擎对此轮与官方句①「都不出手时持权方先结算」相悖");
        std::cout << "  [R3] 双嗑药：权随机判给 " << holder << " 侧 → 账本 ["
                  << g_re_ledger[0] << "," << g_re_ledger[1]
                  << "]（持权方仍后结算；官方句①要求先结算——分歧实录）\n";
    }

    //========== C ★ coverage bit 40/41 运行时用例（头注勘误的行为面） ==========
    {
        CHECK_EQ(state_coverage_bit(State::BATTLE_ROUND_COMPLETION), (1ULL << 40),
                 "C) state_coverage_bit(ROUND_COMPLETION) = bit40");
        CHECK_EQ(state_coverage_bit(State::FINISHED), (1ULL << 41),
                 "C) ★ state_coverage_bit(FINISHED) = bit41 —— 42 项落 bit0..41，"
                 "state.h:61 头注「41 个状态落低 40 位」过期");
        scenario::ScenarioBattle b = make_battle();
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/999);
        CHECK(b.ctx().is_immune(1, ImmunityType::PINK_DAMAGE, State::BATTLE_ROUND_COMPLETION),
              "C) ★ coverage_all 含 bit40（ROUND_COMPLETION）——位图查询通到 40 位");
        CHECK(b.ctx().is_immune(1, ImmunityType::PINK_DAMAGE, State::FINISHED),
              "C) ★ coverage_all 含 bit41（FINISHED）——位图查询通到 41 位（头注「40 位」外）");
        std::cout << "  [C] state_coverage_bit bit40/bit41 编译期钉死 + coverage_all 运行时查询通过\n";
    }

    std::cout << "\n场景 7791 通过：\n"
                 "  R 组 —— ROUND_END 定序实测 = 持先手权方后结算（均出手/一方嗑药与官方句②\n"
                 "  一致；双方嗑药轮引擎先手权随机判定且仍后结算，与官方句①相悖——内核轮\n"
                 "  待修：出手感知分支 + 嗑药轮先手权判定）；\n"
                 "  C 组 —— state.h 覆盖位图实况 42 项落 bit0..41（FINISHED=bit41），头注\n"
                 "  「41 个状态（-1..39）落低 40 位」为过期注释（内核不改，百科记勘误）。\n";
    return 0;
}
