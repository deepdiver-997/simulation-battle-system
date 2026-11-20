// 场景 3850 —— K241 受击双概率异常族（804，soul_lib/k241_hit_dual_anomaly.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 3850-3859 → T3850 一块四例）
//
// 断言口径（合成行 0/100 短路确定性 + 真实行审计一致性）：
//   - a 真实行 [50,50]：冰封申报审计 = 50；级联一致性 = 冰封落地则无麻痹尝试
//     （para 审计 -1），未落地则麻痹申报 50。
//   - b 合成行 [100,100]：冰封必落地（Icebound 活跃）+ 无麻痹（级联门）。
//   - c 合成行 [0,100]：冰封必不落地 → 麻痹申报 100 → 麻痹落地（Paralysis 活跃）。
//   - d 非受击（己方攻击回合）：watcher 静默（对手无任何异常、审计不动）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);
constexpr int kPara = static_cast<int>(AbnormalStatusId::Paralysis);
constexpr int kIceAuditBase = 991442000;   // +owner（实现文件同锚）
constexpr int kParaAuditBase = 991443000;  // +owner

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(804, "K241 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3171;   // 银月狼王
    return sm;
}

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, 99999, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType a1) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, a1, a1 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> real_row{50, 50};

    // ══ T3850.a 真实行审计 + 级联一致性 ══
    {
        ScenarioBattle b(make_party(mark_pet(real_row)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK_EQ(audit_int(ctx, kIceAuditBase + 0), 50, "T3850.a 冰封申报 = db[0]");
        const bool ice_landed = ctx.has_active_abnormal_status(1, kIce);
        if (ice_landed) {
            CHECK_EQ(audit_int(ctx, kParaAuditBase + 0), -1,
                     "T3850.a 冰封落地 → 无麻痹尝试");
        } else {
            CHECK_EQ(audit_int(ctx, kParaAuditBase + 0), 50,
                     "T3850.a 冰封未落地 → 麻痹申报 50");
        }
        std::cout << "  [T3850.a] 真实行审计一致性 通过（ice_landed=" << ice_landed << "）\n";
    }

    // ══ T3850.b 合成行 [100,100]：冰封必落地 + 级联门 ══
    {
        ScenarioBattle b(make_party(mark_pet({100, 100})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(ctx.has_active_abnormal_status(1, kIce), "T3850.b 100% 冰封落地");
        CHECK(!ctx.has_active_abnormal_status(1, kPara), "T3850.b 冰封触发 → 无麻痹");
        CHECK_EQ(audit_int(ctx, kParaAuditBase + 0), -1, "T3850.b 级联门未开");
        std::cout << "  [T3850.b] 100% 冰封+级联门 通过\n";
    }

    // ══ T3850.c 合成行 [0,100]：冰封必不落地 → 麻痹 100% 落地 ══
    {
        ScenarioBattle b(make_party(mark_pet({0, 100})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(!ctx.has_active_abnormal_status(1, kIce), "T3850.c 0% 冰封不落地");
        CHECK_EQ(audit_int(ctx, kParaAuditBase + 0), 100, "T3850.c 级联开 → 麻痹申报 100");
        CHECK(ctx.has_active_abnormal_status(1, kPara), "T3850.c 麻痹 100% 落地");
        std::cout << "  [T3850.c] 0/100 级联麻痹 通过\n";
    }

    // ══ T3850.d 己方攻击回合：watcher 静默 ══
    {
        ScenarioBattle b(make_party(mark_pet({100, 100})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kIce), "T3850.d 非受击不触发");
        CHECK(!ctx.has_active_abnormal_status(1, kPara), "T3850.d 非受击无麻痹");
        CHECK_EQ(audit_int(ctx, kIceAuditBase + 0), -1, "T3850.d 审计未动");
        std::cout << "  [T3850.d] 非受击静默 通过\n";
    }

    std::cout << "scenario_3850 PASS (K241 受击双概率异常族)" << std::endl;
    return 0;
}
