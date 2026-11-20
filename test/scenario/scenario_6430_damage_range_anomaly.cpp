// 场景 6430 —— J310 伤害区间/高于门异常族（748/1539，
// moves_lib/j_damage_range_anomaly.cpp）（2026-10-22，worktree feat/jbatch；号段 6430-6439）
//
// 断言口径（审计槽 + 绝对量带 + 异常槽结构断言）：
//   - 触发面 = 伤害管线 DETECT 相位读 `resolvedDamage.final`：
//     748 = 闭区间 [下限,上限]；1539 = 严格大于阈值；只认落地红伤（isRed）。
//   - 审计槽 991748100 / 991539100 = 触发那一发的 final（-999 = 未触发）；
//     991539101 = 1539 兜底（消除对手回合类效果）触发次数（-999 = 未走兜底）。
//   - 异常落地断言 = `has_active_abnormal_status(对手, 异常码)`（结构断言，非数值）。
//   - 兜底断言 = 对手名下回合类票 `rule_center_.has_round_type(对手)`：
//     免异常时兜底必清票；未触发/未免疫时票必须还在（对照组）。
//   - 伤害带按官方公式反推：damage = (0.84×攻/防×威力+2)×克制1.0×(217+rand%39)/255
//     → 物攻威力160：攻200 → 270.8×[0.851,1.0] → [230,270]（区间 [200,300] 内）；
//     攻100 → [116,136]（区间下沿外）；攻1000 → [1145,1346]（区间上沿外）；
//     攻300 → [344,405]（>300 触发 1539）。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物 40（填充技，无被测效果）
constexpr int k748Carrier = 17196; // 恶·灭世修罗 [33,748]|[200,300,6]（物 160，异常 6=害怕）
constexpr int k1539Carrier = 34797;// 绒羽霜漫天 [1491,1061,1539]|[300,100,15]（物 160，异常 15=冰封）

constexpr int kAnomaly748 = 6;     // 害怕
constexpr int kAnomaly1539 = 15;   // 冰封

constexpr int k748Audit = 991748100;
constexpr int k1539Audit = 991539100;
constexpr int k1539Fallback = 991539101;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier, int atk, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/hp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/hp);
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

// 给**对手名下**（source=1）挂一条回合型封属票——己方可断的"回合类效果"
//（scenario_710 同款注入；封属性技能不影响全攻击技能的测试动作）。
void grant_foe_round_ticket(ScenarioBattle& b) {
    seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70610,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

struct AnomalyTrial {
    int damage = 0;
    int audit = -999;
    int fallback = -999;
    bool anomaly_active = false;
    bool foe_has_round_ticket = false;
};

// 一局：R1 载体技（对手 NONE）。immune=true 时给对手发全异常免疫。
AnomalyTrial run_trial(int carrier, int atk, int audit_key, int anomaly_code, bool immune,
                        bool with_round_ticket) {
    ScenarioBattle b(make_party(mk_owner(carrier, atk)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (immune) {
        // 全异常免疫（anomaly_mask=0 = 全异常）；coverage_all 覆盖任何时点。
        (void)ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                                  /*anomaly_mask=*/0, /*duration_rounds=*/3, /*source_id=*/0);
    }
    if (with_round_ticket) {
        grant_foe_round_ticket(b);
    }
    AnomalyTrial t;
    t.foe_has_round_ticket = ctx.rule_center_.has_round_type(1);
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.damage = before - ctx.getPet(1).hp;
    t.audit = audit(ctx, audit_key);
    t.fallback = audit(ctx, k1539Fallback);
    t.anomaly_active = ctx.has_active_abnormal_status(1, anomaly_code);
    t.foe_has_round_ticket = ctx.rule_center_.has_round_type(1);
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6430.a 748 触发：伤害落在 [200,300] → 对手害怕(6) ══
    {
        const AnomalyTrial t = run_trial(k748Carrier, /*atk=*/200, k748Audit, kAnomaly748,
                                          /*immune=*/false, /*with_round_ticket=*/false);
        std::cout << "  [T6430.a] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active << std::endl;
        CHECK(t.damage >= 225 && t.damage <= 276,
              "T6430.a 载体伤害落在 [230,270] 区间内带（实测 " + std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, t.damage, "T6430.a 审计槽=触发那一发的 final");
        CHECK(t.anomaly_active, "T6430.a 对手挂上害怕(6)");
    }

    // ══ T6430.b 748 门闭：伤害低于下限 → 不触发 ══
    {
        const AnomalyTrial t = run_trial(k748Carrier, /*atk=*/100, k748Audit, kAnomaly748,
                                          /*immune=*/false, /*with_round_ticket=*/false);
        std::cout << "  [T6430.b] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active << std::endl;
        CHECK(t.damage >= 110 && t.damage <= 142,
              "T6430.b 载体伤害落在 [116,136]（区间下沿外，实测 " +
                  std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, -999, "T6430.b 门闭 → 审计未写");
        CHECK(!t.anomaly_active, "T6430.b 门闭 → 对手无异常");
    }

    // ══ T6430.c 748 门闭：伤害高于上限 → 不触发 ══
    {
        const AnomalyTrial t = run_trial(k748Carrier, /*atk=*/1000, k748Audit, kAnomaly748,
                                          /*immune=*/false, /*with_round_ticket=*/false);
        std::cout << "  [T6430.c] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active << std::endl;
        CHECK(t.damage >= 1140 && t.damage <= 1350,
              "T6430.c 载体伤害落在 [1145,1346]（区间上沿外，实测 " +
                  std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, -999, "T6430.c 门闭 → 审计未写");
        CHECK(!t.anomaly_active, "T6430.c 门闭 → 对手无异常");
    }

    // ══ T6430.d 1539 触发：伤害 > 300 → 对手冰封(15) ══
    {
        const AnomalyTrial t = run_trial(k1539Carrier, /*atk=*/300, k1539Audit, kAnomaly1539,
                                          /*immune=*/false, /*with_round_ticket=*/true);
        std::cout << "  [T6430.d] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active
                  << " foe_ticket=" << t.foe_has_round_ticket << std::endl;
        CHECK(t.damage >= 340 && t.damage <= 410,
              "T6430.d 载体伤害落在 [344,405]（>300，实测 " + std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, t.damage, "T6430.d 审计槽=触发那一发的 final");
        CHECK(t.anomaly_active, "T6430.d 对手挂上冰封(15)");
        CHECK(t.foe_has_round_ticket,
              "T6430.d 异常落地 → **不**走兜底（对手回合类票仍在）");
        CHECK_EQ(t.fallback, -999, "T6430.d 兜底计数未写");
    }

    // ══ T6430.e 1539 门闭：伤害 ≤ 300 → 不触发、不走兜底（对手票保留）══
    {
        const AnomalyTrial t = run_trial(k1539Carrier, /*atk=*/100, k1539Audit, kAnomaly1539,
                                          /*immune=*/false, /*with_round_ticket=*/true);
        std::cout << "  [T6430.e] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active
                  << " foe_ticket=" << t.foe_has_round_ticket << std::endl;
        CHECK(t.damage >= 110 && t.damage <= 142,
              "T6430.e 载体伤害落在 [116,136]（≤300，实测 " + std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, -999, "T6430.e 门闭 → 审计未写");
        CHECK(!t.anomaly_active, "T6430.e 门闭 → 对手无异常");
        CHECK(t.foe_has_round_ticket, "T6430.e 门闭 → 对手回合类票保留（未走兜底）");
        CHECK_EQ(t.fallback, -999, "T6430.e 兜底计数未写");
    }

    // ══ T6430.f 1539 兜底：对手全异常免疫 → 异常未落地 → 消除对手回合类效果 ══
    {
        const AnomalyTrial t = run_trial(k1539Carrier, /*atk=*/300, k1539Audit, kAnomaly1539,
                                          /*immune=*/true, /*with_round_ticket=*/true);
        std::cout << "  [T6430.f] damage=" << t.damage << " audit=" << t.audit
                  << " anomaly_active=" << t.anomaly_active
                  << " fallback=" << t.fallback
                  << " foe_ticket=" << t.foe_has_round_ticket << std::endl;
        CHECK(t.damage >= 340 && t.damage <= 410,
              "T6430.f 载体伤害落在 [344,405]（>300，实测 " + std::to_string(t.damage) + "）");
        CHECK_EQ(t.audit, t.damage, "T6430.f 审计槽=触发那一发的 final");
        CHECK(!t.anomaly_active, "T6430.f 对手全异常免疫 → 异常未落地");
        CHECK_EQ(t.fallback, 1, "T6430.f 兜底触发 1 次");
        CHECK(!t.foe_has_round_ticket, "T6430.f 兜底 → 对手回合类效果被消除");
    }

    std::cout << "scenario_6430 PASS (J310 伤害区间/高于门异常族)" << std::endl;
    return 0;
}
