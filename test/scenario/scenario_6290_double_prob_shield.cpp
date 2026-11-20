// 场景 6290 —— J296 双概率异常/护盾兜底族（1582，moves_lib/j_double_prob_shield.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6290-6299）
//
// 断言口径（三段确定性驱动，不靠种子）：
//   - 落地档用 carrier 自带的 100%（37249 焰坠留痕 [100,2,100,2,300]）。
//   - "未触发"档用 **概率闸门** 把申报概率改写成 0（threshold=101/below=0，
//     只管辖 Skill 来源）→ 原语返回 ROLL_FAILED。
//   - 免疫档用 ANOMALY 免疫票 → TARGET_IMMUNE：**不是**"未触发"（ApplyAnomalyResult
//     语义 + J209/J210 口径），因此即使两段都被免疫也**不**走兜底。
//   - 每档都断言两段原始结果码审计槽、盾槽绝对量、fallback 标志与异常存在性。
#include <iostream>
#include <string>
#include <any>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1582 = 37249;   // 焰坠留痕 [1582]|[100,2,100,2,300]（纯链，属性技）
constexpr int kPaiDa = 10004;         // 拍打（占位）
constexpr int kBurn = 2;
constexpr int kGateSource = 7910;
constexpr int kImmuneSource = 7911;
constexpr int kMaxHp = 99999;

constexpr int kResult1Key = 991582100;   // +owner
constexpr int kResult2Key = 991582101;   // +owner
constexpr int kShieldKey = 991582200;    // +owner
constexpr int kFallbackKey = 991582300;  // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_pet(int id, const std::string& name, const std::array<int, 5>& skills, int speed) {
    ElfPet p = make_pet(id, name, skills, speed, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -999;
}

int shield_total(BattleContext& ctx, int side) {
    return ctx.getPet(side).shield_bank_.total();
}

// 把 side 侧受到的概率施加一律改写成 0（ROLL_FAILED）。
void force_roll_failed(BattleContext& ctx, int side) {
    ctx.grant_chance_gate(side, kGateSource + side, ChanceTag::AnomalyAttach,
                          /*threshold_pct=*/101, /*below_result=*/0, /*above_result=*/-1,
                          /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                          /*rounds=*/0);
}

void force_anomaly_immune(BattleContext& ctx, int side) {
    ctx.grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/kImmuneSource + side,
                       /*soul_immunity=*/true);
}

// 一回合：己方施放 carrier，对手 NONE。
void run_carrier(ScenarioBattle& b) {
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6290.a 两段 100% 都落地 → 无兜底盾、双方烧伤 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kCarrier1582, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        run_carrier(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(stor(ctx, kResult1Key + 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6290.a 对手段 SUCCESS");
        CHECK_EQ(stor(ctx, kResult2Key + 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6290.a 自身段 SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T6290.a 对手烧伤");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T6290.a 自身烧伤");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6290.a 不走兜底");
        CHECK_EQ(stor(ctx, kShieldKey + 0), 0, "T6290.a 授予盾量 0");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6290.a 护盾槽 0");
    }

    // ══ T6290.b 两段都被闸门压成 ROLL_FAILED → 兜底盾 300 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kCarrier1582, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_roll_failed(ctx, 0);
        force_roll_failed(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kResult1Key + 0),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T6290.b 对手段 ROLL_FAILED");
        CHECK_EQ(stor(ctx, kResult2Key + 0),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T6290.b 自身段 ROLL_FAILED");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 1, "T6290.b 走兜底");
        CHECK_EQ(stor(ctx, kShieldKey + 0), 300, "T6290.b 授予盾量 = {4}=300");
        CHECK_EQ(shield_total(ctx, 0), 300, "T6290.b 护盾槽 300");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "T6290.b 对手无烧伤");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T6290.b 自身无烧伤");
    }

    // ══ T6290.c 仅对手段未触发、自身段落地 → 不走兜底 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kCarrier1582, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_roll_failed(ctx, 1);   // 只有对手侧被压
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kResult1Key + 0),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T6290.c 对手段未触发");
        CHECK_EQ(stor(ctx, kResult2Key + 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6290.c 自身段落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T6290.c 自身烧伤");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6290.c 一段触发 → 不兜底");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6290.c 护盾槽 0");
    }

    // ══ T6290.d 仅自身段未触发、对手段落地 → 不走兜底 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kCarrier1582, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_roll_failed(ctx, 0);   // 只有自身侧被压
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kResult1Key + 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6290.d 对手段落地");
        CHECK_EQ(stor(ctx, kResult2Key + 0),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T6290.d 自身段未触发");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T6290.d 对手烧伤");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6290.d 一段触发 → 不兜底");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6290.d 护盾槽 0");
    }

    // ══ T6290.e 两段都被免疫 → TARGET_IMMUNE 不是"未触发" → 不兜底 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kCarrier1582, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        force_anomaly_immune(ctx, 0);
        force_anomaly_immune(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kResult1Key + 0),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE), "T6290.e 对手段被免疫");
        CHECK_EQ(stor(ctx, kResult2Key + 0),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE), "T6290.e 自身段被免疫");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "T6290.e 对手无烧伤");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T6290.e 自身无烧伤");
        CHECK_EQ(stor(ctx, kFallbackKey + 0), 0, "T6290.e 被免疫 ≠ 未触发（不兜底）");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6290.e 护盾槽 0");
    }

    std::cout << "scenario 6290 OK" << std::endl;
    return 0;
}
