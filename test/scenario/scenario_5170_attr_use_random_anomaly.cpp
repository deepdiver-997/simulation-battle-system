// 场景 5170 —— J195 属性技异常窗族（1394，moves_lib/j_attr_use_random_anomaly.cpp）
//（2026-10-22，worktree ../agents/j195；号段 5170-5179）
//
// 断言口径：
//   - 1394（追猎 28230 纯链 [1394]|[3,1]，属性技）：R1 先手施放（开 3 回合窗），
//     对手随后使用属性技（高速移动 20017）→ 随机附加 1 种异常给对手。
//     审计槽 == 1（实际落地种数），并扫描全异常槽确认对手确实处于异常状态。
//   - 对照：不施放载体 → 对手用属性技不触发（审计槽缺席、对手无异常）。
//   - 攻击技不计入窗：额外块中对手使用攻击技（拍打）→ 审计槽仍缺席。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40（攻击技对照）
constexpr int kZhuiLie  = 28230;  // [1394]|[3,1]（属性技；3 回合内对手用属性技 → 随机 1 异常）
constexpr int kGaoSu    = 20017;  // 高速移动：属性技（触发窗）
constexpr int kAuditKey = 990139400;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kZhuiLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int first_skill) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int audit(const BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

int active_anomaly_count(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5170 1394 对手使用属性技 → 随机附加 {1} 种异常 ══
    {
        {   // (a) 开窗 + 对手用属性技 → 随机 1 异常落地
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kGaoSu)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 开窗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 属性技
            b.drive();
            CHECK_EQ(audit(ctx), 1,
                     "T5170.a 1394：属性技触发窗，落地 1 种（审计 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(active_anomaly_count(ctx, 1) >= 1,
                  "T5170.a 1394：对手确实处于异常状态（实测种数 " +
                      std::to_string(active_anomaly_count(ctx, 1)) + "）");
        }
        {   // (b) 对照：不施放 → 对手属性技不触发
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kGaoSu)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(audit(ctx), -1, "T5170.b 1394 对照：未开窗 → 审计槽缺席");
            CHECK_EQ(active_anomaly_count(ctx, 1), 0,
                     "T5170.b 1394 对照：对手无异常（实测 " +
                         std::to_string(active_anomaly_count(ctx, 1)) + "）");
        }
        {   // (c) 窗只认属性技：对手使用攻击技（拍打）→ 不触发
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(audit(ctx), -1, "T5170.c 1394：对手攻击技不触发窗（审计槽缺席）");
        }
        std::cout << "  [T5170] 1394 属性技异常窗 + 对照/攻击技 通过\n";
    }

    std::cout << "scenario_5170 PASS (J195 属性技异常窗族)" << std::endl;
    return 0;
}
