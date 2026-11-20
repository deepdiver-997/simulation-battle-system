// 场景 108 —— 睡眠「受击清醒」× 出手顺序（2026-09-21 用户游戏实测口径）
//
// 官方机制（effect_des 8 睡眠：「对手使用攻击技能且未miss时睡眠解除」——控制类，禁行动）：
//   * 我方处于睡眠 + **对手先手**用攻击技能打中我 → 我在对方的 ON_SKILL_HIT 时点被
//     **打醒**（anomaly_on_attack_hit_hook，battleFsm.cpp）→ 轮到我方的行动开始判定
//     （should_skip_action_flow → has_blocking_control_status）时身上已无控制 →
//     **后手正常出手**（前提：没有被附加新的控制类异常）。
//   * 我方处于睡眠 + **我方被判先手** → 行动开始判定时睡眠还在 → **本回合无法出手**
//     （主流程被跳过）；对方随后打我 → 醒了但本回合已经没了。
//   ⇒ 区分轴与场景 105 的"解异常时点"同构：**清醒发生在谁的行动期里**。
//
// 引擎现状：两条机制（受击清醒 hook + 行动开始控制判定）都已有，本场景是**回归锁**。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;          // 飞空俯冲（必中、无 side_effect）
constexpr int kDragonBreath = 36471; // 镜·灼渊龙息（必中攻击 + 100% 焚烬 22）
constexpr int kZhuangJi = 10001;
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);             // 8（控制类）
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);   // 22（控制类）

std::array<int, 5> lead(int id) { return {id, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi}; }
std::array<int, 5> same(int id) { return {id, id, id, id, id}; }

ElfPet sleeper(const std::array<int, 5>& sk, int speed, int hp = 9000) {
    return make_pet(4586, "睡眠方", sk, speed, /*atk=*/100, hp);
}

ElfPet attacker(const std::array<int, 5>& sk, int speed, int atk = 100) {
    return make_pet(1, "攻方", sk, speed, atk, 9000);
}

std::array<ElfPet, 6> party0_with(const ElfPet& own) {
    std::array<ElfPet, 6> party = make_party(attacker(same(kZhuangJi), 100));
    party[0] = own;
    return party;
}

bool slot_was_used(const BattleContext& ctx, int slot) {
    return ctx.seerRobot[0].elfPets[ctx.on_stage[0]].skills[slot].pp
        < ctx.seerRobot[0].elfPets[ctx.on_stage[0]].skills[slot].maxPP;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 睡眠 + 对手先手攻击 → 被打醒 → 后手正常出手
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party0_with(sleeper(lead(kHit), /*speed=*/50)),
                         make_party(attacker(lead(kHit), /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/0, kSleep, /*dur=*/3, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS), "A) 前置：我方已入睡");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "A) 前置：对手先手");
        CHECK_EQ(ctx.abnormal_status_end_round[0][kSleep], 0,
                 "A1) ★ 被对手的攻击技能打中 → 睡眠解除（受击清醒）");
        CHECK(slot_was_used(ctx, 0),
              "A2) ★ 醒在**我方行动开始判定之前**（对手行动期内）→ 后手正常出手");
        std::cout << "  [A] 睡眠·对手先手打醒后正常出手 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 睡眠 + 己方先手 → 行动开始判定时还在睡 → 本回合无法出手；随后被打醒但已晚了
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party0_with(sleeper(lead(kHit), /*speed=*/200)),
                         make_party(attacker(lead(kHit), /*speed=*/50)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/0, kSleep, /*dur=*/3, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS), "B) 前置：我方已入睡");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "B) 前置：我方先手");
        CHECK(!slot_was_used(ctx, 0),
              "B1) ★ 行动开始判定时睡眠还在 → 主流程被跳过，本回合无法出手");
        CHECK_EQ(ctx.abnormal_status_end_round[0][kSleep], 0,
                 "B2) 对方随后的攻击把我打醒——但本回合行动权已经没了");
        std::cout << "  [B] 睡眠·先手被跳过后打醒已晚 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 打醒的同时被附加新的控制类异常 → 依旧不能出手（"前提是没有新控制"）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party0_with(sleeper(lead(kHit), /*speed=*/50)),
                         make_party(attacker(same(kDragonBreath), /*speed=*/200, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/0, kSleep, /*dur=*/3, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS), "C) 前置：我方已入睡");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.abnormal_status_end_round[0][kSleep], 0,
                 "C1) 睡眠照常被攻击打醒");
        CHECK(ctx.abnormal_status_end_round[0][kIncinerate] != 0,
              "C2) 前置：同一击又附加了焚烬（控制类）");
        CHECK(!slot_was_used(ctx, 0),
              "C3) ★ 新的控制类异常落地 → 行动开始判定依旧不过 → 本回合不能出手");
        std::cout << "  [C] 睡眠·打醒但新控制落地仍不能出手 通过\n";
    }

    std::cout << "scenario_108 PASS（睡眠受击清醒 × 出手顺序：后手打醒可出手 / 先手跳过 / 新控制仍拦）"
              << std::endl;
    return 0;
}
