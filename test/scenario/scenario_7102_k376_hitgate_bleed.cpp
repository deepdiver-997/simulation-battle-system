#include <utils/sbs_rand.h>
// 场景 7102 —— K376 受伤阈值门+概率流血族（340，soul_lib/k_hitgate_bleed.cpp）
//（2026-10-24，worktree ../agents/k376；号段 7102 → T7102.a/.b/.c 三块）
//
// 断言口径（真实攻击走伤害管线；概率走审计槽精确计数）：
//   - a 低伤门+30% 流血：atk=1 拍打（伤 1~2 < 250）20 回合，审计槽（990376801）
//     计"落地次数" ∈ [2,14]（p=0.3/n=20 → 期望 6；下界拦 0%/3% 误档、上界拦
//     100% 误档）+ 流血状态至少观测到一次（Bleed=16 引擎已建模 tick）；
//   - b 高伤不置门：atk=5000 刀（[1450,1700] ≥ 250）→ 无审计写入（未掷点）、
//     无流血状态、旗缺席；
//   - c 旗一次性：r1 低伤（旗置→ROUND_END 消费清旗）→ r2 高伤：审计结果不变
//     （r2 未掷点）、旗保持缺席。
//   ⚠️ 概率断言跑前 srand 定种子（触发面 rand 序固定 → 逐次一致）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler
constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);   // 16
constexpr int kMaxHp = 800000;
constexpr int kAuditPctKey = 990376800;      // +owner*2（owner=0 → 990376800）
constexpr int kAuditResultKey = 990376801;
constexpr int kPendingFlagKey = 990376500;   // +owner

SoulMark make_mark() {
    SoulMark sm(/*effect_id=*/340, "K340 魂印", "test",
                EffectArgs(std::vector<int>{250, 30, 16}));
    sm.monster_id = 2701;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p);
    return p;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

bool anomaly_landed(BattleContext& ctx) {
    const int res = audit_int(ctx, kAuditResultKey);
    return res == static_cast<int>(ApplyAnomalyResult::SUCCESS)
        || res == static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED)
        || res == static_cast<int>(ApplyAnomalyResult::REPLACED_EXISTING);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7102.a 低伤门 + 30% 流血（审计槽精确计数）═══════════════════════════
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int landed = 0;
        bool bleed_seen = false;
        for (int r = 0; r < 20; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (anomaly_landed(ctx)) {
                ++landed;
            }
            if (ctx.has_active_abnormal_status(1, kBleed)) {
                bleed_seen = true;
            }
        }
        CHECK(audit_int(ctx, kAuditPctKey) == 30,
              "T7102.a 审计槽申报概率应=30（实测 " << audit_int(ctx, kAuditPctKey)
                  << "）");
        CHECK(landed >= 2 && landed <= 14,
              "T7102.a 20 回合 30% 流血落地次数 ∈ [2,14]（期望 6，实测 "
                  + std::to_string(landed) + "）");
        CHECK(bleed_seen, "T7102.a 流血状态至少观测一次（Bleed=16 tick 已建模）");
        std::cout << "  [T7102.a] 340 低伤门+30%流血（审计计数 " << landed
                  << "/20）通过\n";
    }

    // ══ T7102.b 高伤不置门 ═══════════════════════════════════════════════════
    {
        sbs_srand(20261025);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int loss = hp_before - ctx.getPet(0).hp;
        CHECK(loss >= 250, "T7102.b 高伤前提不成立：loss=" << loss);
        CHECK(ctx.plugin_storage.find(kAuditResultKey) == ctx.plugin_storage.end(),
              "T7102.b 高伤不应掷点（无审计写入）");
        CHECK(ctx.plugin_storage.find(kPendingFlagKey) == ctx.plugin_storage.end(),
              "T7102.b 高伤不应置旗");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed),
              "T7102.b 高伤回合对手不应流血");
        std::cout << "  [T7102.b] 340 高伤不置门（loss=" << loss << "）通过\n";
    }

    // ══ T7102.c 旗一次性消费 ═════════════════════════════════════════════════
    {
        sbs_srand(20261026);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        // r1：低伤 → 旗置 → ROUND_END 消费清旗（30% 掷点一次）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.plugin_storage.find(kPendingFlagKey) == ctx.plugin_storage.end(),
              "T7102.c r1 旗应在 ROUND_END 消费清旗");
        CHECK(audit_int(ctx, kAuditPctKey) == 30, "T7102.c r1 应掷点一次");
        const int result_after_r1 = audit_int(ctx, kAuditResultKey);
        // r2：拉高对手攻面板 → 高伤 → 不置旗、不掷点
        ctx.getPet(1).numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] = 5000;
        ctx.getPet(1).numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 5000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int loss2 = hp_before - ctx.getPet(0).hp;
        CHECK(loss2 >= 250, "T7102.c r2 高伤前提不成立：loss=" << loss2);
        CHECK(ctx.plugin_storage.find(kPendingFlagKey) == ctx.plugin_storage.end(),
              "T7102.c r2 高伤 → 旗保持缺席");
        CHECK(audit_int(ctx, kAuditResultKey) == result_after_r1,
              "T7102.c r2 高伤 → 不掷点（审计结果不变）");
        std::cout << "  [T7102.c] 340 旗一次性消费 通过\n";
    }

    std::cout << "scenario_7102 PASS (K376 受伤阈值门+概率流血族)" << std::endl;
    return 0;
}
