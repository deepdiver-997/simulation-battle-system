// 场景 5700 —— J239 致命一击概率族（1429/1420，moves_lib/j_prob_must_crit.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5700-5709）
//
// 断言口径（统计带宽 + 互斥恒等式 + 审计槽 + 伤害比值）：
//   - 暴击掷点在 ON_SKILL_HIT（早于 SKILL_EFFECT），本族走 D26 1395 通道直接置
//     `ctx->crit_happened`；ATTACK_DAMAGE 只消费旗标并按 cached_crit_damage 放大。
//   - 1429 [100,8]（38520 惊人注意力）：每局"暴击 XOR 睡眠"必居其一（{0}=100 ⇒
//     未触发必上异常）→ 断言 crit+anomaly == N 且 both == 0。
//   - 1429 [50,20]（33135 茶点暴击）：crit ≈ 50%（带 25%~75%），blind ≈ 25%
//     （非暴击 50% × 申报 50%；带 [5%,50%]），both == 0。
//   - 1420（33281 漫天霜花）：自身有正等级 → crit ≈ 50%；无正等级 → crit == 0
//     （绝对值门闭，不写"与对照相等"）；审计槽 -1 证明门真的拦住。
//   - 伤害比值：同载体 crit 局均伤 / 非暴击局均伤 ∈ [1.7,2.3]（暴击系数 200 实测；
//     85%~100% 浮动由均值抹平）。
//   - 审计槽逐局校验：991429100 = roll 结果与 crit_happened 一致；
//     991420100 = -1（门闭）/0（roll 未中）/1（暴击）。
#include <iostream>
#include <any>
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

constexpr int kPaiDa    = 10004;  // 拍打：物 40
constexpr int kChaDian  = 33135;  // [1429]|[50,20]（50% 暴击 / 未触发 50% 失明）
constexpr int kJingRen  = 38520;  // [1429]|[100,8]（50% 暴击 / 未触发 100% 睡眠）
constexpr int kManTian  = 33281;  // [1420][]（有提升时 50% 必暴）
constexpr int kAnomalyBlind = 20;
constexpr int kAnomalySleep = 8;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
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

struct Trial {
    bool crit = false;
    bool anomaly = false;
    int damage = 0;
    int roll_audit = -999;
    int result_audit = -999;
    int audit_1420 = -999;
    bool is_crit_snapshot = false;
};

Trial run_trial(int carrier, int anomaly_id, bool boosted, unsigned seed) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.ctx().seed_rng(seed);
    b.start();
    BattleContext& ctx = b.ctx();
    if (boosted) {
        ctx.ability_levels[0][0] = 1;   // "处于能力提升状态"门
    }
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Trial t;
    t.crit = ctx.crit_happened[0];
    t.is_crit_snapshot = ctx.resolvedDamage.isCrit;
    t.damage = foe_before - ctx.getPet(1).hp;
    t.roll_audit = audit(ctx, 991429100);
    t.result_audit = audit(ctx, 991429110);
    t.audit_1420 = audit(ctx, 991420100);
    if (anomaly_id > 0) {
        t.anomaly = core_api().has_active_anomaly(&ctx, 1, anomaly_id);
    }
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5700 1429 [100,8]：暴击 XOR 睡眠（互斥 + 全覆盖） ══
    {
        constexpr int N = 80;
        int crit = 0, anomaly = 0, both = 0, neither = 0, mismatch = 0;
        long crit_dmg = 0, plain_dmg = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kJingRen, kAnomalySleep, /*boosted=*/false, 1 + i * 104729u);
            crit += t.crit ? 1 : 0;
            anomaly += t.anomaly ? 1 : 0;
            both += (t.crit && t.anomaly) ? 1 : 0;
            neither += (!t.crit && !t.anomaly) ? 1 : 0;
            mismatch += (t.crit != (t.roll_audit == 1)) ? 1 : 0;
            if (t.crit) {
                crit_dmg += t.damage;
            } else {
                plain_dmg += t.damage;
            }
        }
        std::cout << "  [T5700.a] 1429 [100,8] N=" << N << " crit=" << crit
                  << " sleep=" << anomaly << " both=" << both << " neither=" << neither
                  << std::endl;
        CHECK(mismatch == 0, "T5700.a 审计 roll 与 crit_happened 逐局一致（mismatch=" +
                                 std::to_string(mismatch) + "）");
        CHECK(both == 0, "T5700.a 暴击与异常互斥（both=" + std::to_string(both) + "）");
        CHECK(neither == 0, "T5700.a {0}=100 ⇒ 非暴击局必上睡眠（neither=" +
                                std::to_string(neither) + "）");
        CHECK(crit + anomaly == N, "T5700.a 暴击 XOR 睡眠覆盖全部局（crit+sleep=" +
                                       std::to_string(crit + anomaly) + "/" + std::to_string(N) + "）");
        CHECK(crit >= N * 3 / 10 && crit <= N * 7 / 10,
              "T5700.a 暴击率 ~50%（实测 " + std::to_string(crit) + "/" + std::to_string(N) + "）");
        CHECK(anomaly >= N * 3 / 10 && anomaly <= N * 7 / 10,
              "T5700.a 非暴击局睡眠 ~50%（实测 " + std::to_string(anomaly) + "/" +
                  std::to_string(N) + "）");
        // 伤害比值：crit 局均伤 / 非暴击局均伤 ≈ 2（暴击系数 200）。
        const double ratio = (static_cast<double>(crit_dmg) / crit)
                             / (static_cast<double>(plain_dmg) / anomaly);
        std::cout << "  [T5700.a] crit_avg=" << (crit_dmg / static_cast<long>(crit))
                  << " plain_avg=" << (plain_dmg / static_cast<long>(anomaly))
                  << " ratio=" << ratio << std::endl;
        CHECK(ratio > 1.7 && ratio < 2.3,
              "T5700.a 暴击伤害 ≈2×（实测 ratio=" + std::to_string(ratio) + "）");
    }

    // ══ T5700 1429 [50,20]：双段概率（crit≈50%，失明≈25%） ══
    {
        constexpr int N = 100;
        int crit = 0, blind = 0, both = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kChaDian, kAnomalyBlind, /*boosted=*/false, 1 + i * 104729u);
            crit += t.crit ? 1 : 0;
            blind += t.anomaly ? 1 : 0;
            both += (t.crit && t.anomaly) ? 1 : 0;
        }
        std::cout << "  [T5700.b] 1429 [50,20] N=" << N << " crit=" << crit
                  << " blind=" << blind << " both=" << both << std::endl;
        CHECK(both == 0, "T5700.b 暴击与失明互斥（both=" + std::to_string(both) + "）");
        CHECK(crit >= N * 3 / 10 && crit <= N * 7 / 10,
              "T5700.b 暴击率 ~50%（实测 " + std::to_string(crit) + "/" + std::to_string(N) + "）");
        CHECK(blind >= N / 20 && blind <= N / 2,
              "T5700.b 失明率 ~25%（实测 " + std::to_string(blind) + "/" + std::to_string(N) + "）");
    }

    // ══ T5700 1420：有提升门 → 50% 必暴；无提升 → 0（绝对值） ══
    {
        constexpr int N = 60;
        int crit_boosted = 0, crit_plain = 0, audit_ok = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kManTian, 0, /*boosted=*/true, 1 + i * 104729u);
            crit_boosted += t.crit ? 1 : 0;
            audit_ok += (t.audit_1420 == (t.crit ? 1 : 0)) ? 1 : 0;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kManTian, 0, /*boosted=*/false, 1 + i * 104729u);
            crit_plain += t.crit ? 1 : 0;
            audit_ok += (t.audit_1420 == -1) ? 1 : 0;
        }
        std::cout << "  [T5700.c] 1420 boosted crit=" << crit_boosted << "/" << N
                  << " plain crit=" << crit_plain << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (2 * N) << std::endl;
        CHECK(crit_boosted >= N * 3 / 10 && crit_boosted <= N * 7 / 10,
              "T5700.c 有提升 → 暴击率 ~50%（实测 " + std::to_string(crit_boosted) + "/" +
                  std::to_string(N) + "）");
        CHECK(crit_plain == 0, "T5700.c 无提升 → 必暴门闭（实测 " + std::to_string(crit_plain) + "）");
        CHECK(audit_ok == 2 * N, "T5700.c 审计槽逐局吻合（实测 " + std::to_string(audit_ok) +
                                      "/" + std::to_string(2 * N) + "）");
    }

    std::cout << "scenario_5700 PASS (J239 致命一击概率族)" << std::endl;
    return 0;
}
