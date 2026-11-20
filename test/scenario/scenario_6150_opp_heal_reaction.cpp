#include <utils/sbs_rand.h>
// 场景 6150 —— J282 对手回血反应固伤窗族（1071，moves_lib/j_opp_heal_reaction.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6150-6159 → T6150.a-d）
//
// 断言口径：
//   - 载体 火焰跃动 26321（属性技，1071=[3,2,300]）→ 3 回合外窗；对手回血后
//     2 回合内自身攻击附加 300 点固定伤害。
//   - T6150.a 正样本：第 0 回合挂窗 → 对手 heal_amount(1000) 触发外窗（审计 1）→
//     第 1/2 回合自身拍打：审计附加 2 次，且与"同 seed 无回血对照局"的红伤
//     完全一致、掉血差恰为 300（固定伤害绝对值，坑集 §2）；第 3 回合差为 0（窗过期）。
//   - T6150.b 窗外回血：3 回合后再 heal → 外窗已 cleanup，审计触发保持 0、攻击不附加。
//   - T6150.c 药剂除外：对手嗑体力药剂（直写本体、不走 heal_impl）→ 触发审计 0，
//     后续攻击不附加（官方"药剂恢复除外"由事件面天然满足）。
//   - T6150.d 只认对手：自身回血不触发外窗。
#include <any>
#include <iostream>
#include <map>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 26321;   // 火焰跃动：1071=[3,2,300]
constexpr int kFiller  = 10004;   // 拍打
constexpr int kMaxHp   = 99999;
constexpr int kMedHpPotion = 300011;   // 初级体力药剂（+20）
constexpr int kFixed = 300;

constexpr int kTriggerAuditBase = 991071000;
constexpr int kAppendAuditBase  = 991071100;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe() {
    ElfPet p = make_pet(2, "对手", {kFiller, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// self_slot = -1 → 己方 NONE；否则用该槽技能。对手固定用 filler。
void one_round(ScenarioBattle& b, int self_slot) {
    if (self_slot >= 0) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    } else {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    }
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 对照/实验同 seed 的 red 伤害序列：第 0 回合挂窗，第 1..3 回合拍打。
// heal_at = -1 表示不回血；否则在第 heal_at 回合结束后回 1000。
std::array<int, 4> run_attack_series(unsigned seed, int heal_round) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(1).hp = 50000;   // 压当前体力（make_pet 的 hp=max，坑集 §4）
    std::array<int, 4> loss{};
    for (int r = 0; r < 4; ++r) {
        const int before = ctx.getPet(1).hp;
        one_round(b, r == 0 ? 0 : 1);
        loss[static_cast<std::size_t>(r)] = before - ctx.getPet(1).hp;
        if (r == heal_round) {
            (void)core_api().heal_amount(&ctx, 1, 1000);
        }
    }
    return loss;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6150.a 第 0 回合后回血 → 第 1/2 回合攻击附加 300 ══
    {
        // 实验局与对照局同 seed、同操作；差 = 固定伤害。
        const auto ctrl = run_attack_series(20261022u, -1);
        sbs_srand(20261022u);
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 50000;
        int exp_loss[4] = {0, 0, 0, 0};
        for (int r = 0; r < 4; ++r) {
            const int before = ctx.getPet(1).hp;
            one_round(b, r == 0 ? 0 : 1);
            exp_loss[r] = before - ctx.getPet(1).hp;
            if (r == 0) {
                (void)core_api().heal_amount(&ctx, 1, 1000);
            }
            if (r == 1) {
                // 事件是"入队+晚 drain"：回血事件在下一回合首个时点桶后才投递。
                CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 1,
                         "T6150.a 1071：对手回血 → 外窗触发 1 次");
            }
        }
        CHECK_EQ(exp_loss[1] - ctrl[1], kFixed,
                 "T6150.a 1071：第 1 回合附加 300 固定伤害（差="
                     + std::to_string(exp_loss[1] - ctrl[1]) + "）");
        CHECK_EQ(exp_loss[2] - ctrl[2], kFixed,
                 "T6150.a 1071：第 2 回合附加 300 固定伤害（差="
                     + std::to_string(exp_loss[2] - ctrl[2]) + "）");
        CHECK_EQ(exp_loss[3], ctrl[3], "T6150.a 1071：第 3 回合内窗过期 → 无附加");
        CHECK_EQ(audit_value(ctx, kAppendAuditBase + 0), 2,
                 "T6150.a 1071：内窗附加恰好 2 次");
        std::cout << "  [T6150.a] 回血反应附加 通过 (d1=" << exp_loss[1] - ctrl[1]
                  << ", d2=" << exp_loss[2] - ctrl[2] << ")\n";
    }

    // ══ T6150.b 外窗过期后回血 → 不触发 ══
    {
        sbs_srand(777u);
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 50000;
        one_round(b, 0);   // r0 挂 3 回合外窗
        one_round(b, -1);  // r1
        one_round(b, -1);  // r2；回合末 cleanup：currentRound - 0 >= 3 → 过期
        (void)core_api().heal_amount(&ctx, 1, 1000);
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6150.b 1071：窗外回血不触发");
        one_round(b, 1);
        CHECK_EQ(audit_value(ctx, kAppendAuditBase + 0), 0,
                 "T6150.b 1071：无内窗 → 攻击不附加");
        std::cout << "  [T6150.b] 窗外回血 通过\n";
    }

    // ══ T6150.c 药剂恢复除外：嗑药不回血事件 ══
    {
        MedicineStock foe_med{{kMedHpPotion, 1}};
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()), {}, foe_med);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 50000;
        one_round(b, 0);   // r0 挂窗
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 50020, "T6150.c 前置：药剂 +20 已生效");
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6150.c 1071：药剂恢复不触发外窗");
        one_round(b, 1);
        CHECK_EQ(audit_value(ctx, kAppendAuditBase + 0), 0,
                 "T6150.c 1071：药剂恢复不产生附加窗");
        std::cout << "  [T6150.c] 药剂除外 通过\n";
    }

    // ══ T6150.d 只认"对手"回血：自身回血不触发 ══
    {
        sbs_srand(999u);
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 50000;
        one_round(b, 0);   // r0 挂窗
        ctx.getPet(0).hp = 50000;
        (void)core_api().heal_amount(&ctx, 0, 1000);   // 自身回血
        CHECK_EQ(ctx.getPet(0).hp, 51000, "T6150.d 前置：自身回血已落地");
        one_round(b, 1);   // 事件在本回合首个时点后投递
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6150.d 1071：自身回血不触发外窗（只认对手）");
        CHECK_EQ(audit_value(ctx, kAppendAuditBase + 0), 0,
                 "T6150.d 1071：自身回血不产生附加窗");
        std::cout << "  [T6150.d] 只认对手回血 通过\n";
    }

    std::cout << "scenario_6150 PASS (J282 对手回血反应固伤窗族)" << std::endl;
    return 0;
}
