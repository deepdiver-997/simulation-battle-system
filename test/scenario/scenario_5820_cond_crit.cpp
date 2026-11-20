// 场景 5820 —— J251 条件必暴族（1639/1590，moves_lib/j_cond_crit.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5820-5829）
//
// 断言口径（门闭用绝对值 + 审计槽 + 暴击伤害比值）：
//   - 暴击掷点在 ON_SKILL_HIT（早于 SKILL_EFFECT），本族走 D26 1395 通道直接置
//     `ctx->crit_happened`；ATTACK_DAMAGE 只消费旗标并按 cached_crit_damage(200) 放大。
//   - 1639（35711 拥荧入怀，链 [1639,991]）：自身任一正等级 → 必暴；无正等级 → 必不暴
//     （绝对值 crit==0，不用"与对照相等"）；审计槽 991639100 ∈ {1,-1} 逐局吻合。
//   - 1590（34950 神鸟扑击，纯链）：自身 hp < 对手 hp → 必暴；hp == 对手 或 hp > 对手
//     → 必不暴（严格小于，相等不触发）；审计槽 991590100 ∈ {1,-1} 逐局吻合。
//   - 伤害比值：同载体"必暴局均伤 / 非暴局均伤" ∈ [1.7,2.3]（85%~100% 浮动由均值抹平）。
//   - 低体构造按坑集 §4：make_pet 的 hp 同时是 max，满血构造后单独压当前 hp。
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

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充）
constexpr int k1639   = 35711;  // 拥荧入怀 [1639,991]（991=未击败闪避窗，对手 NONE 不跑）
constexpr int k1590   = 34950;  // 神鸟扑击 [1590]（纯链）

constexpr int k1639Audit = 991639100;
constexpr int k1590Audit = 991590100;

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
    bool is_crit_snapshot = false;
    int damage = 0;
    int audit = -999;
};

// mode: 0=1639 无提升 / 1=1639 有提升 / 2=1590 低体 / 3=1590 等高 / 4=1590 高体
Trial run_trial(int carrier, int mode) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (mode == 1) {
        ctx.ability_levels[0][0] = 1;   // "处于能力提升状态"门（攻 +1）
    }
    if (mode == 2) {
        ctx.getPet(0).hp = 1000;        // 当前体力压到对手之下（max 仍 99999）
    }
    if (mode == 4) {
        ctx.getPet(1).hp = 1000;        // 对手压到自身之下（自身高体）
    }
    const int foe_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Trial t;
    t.crit = ctx.crit_happened[0];
    t.is_crit_snapshot = ctx.resolvedDamage.isCrit;
    t.damage = foe_before - ctx.getPet(1).hp;
    t.audit = audit(ctx, (carrier == k1639 ? k1639Audit : k1590Audit));
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5820.a 1639：有提升 → 必暴；无提升 → 必不暴（绝对门闭） ══
    {
        constexpr int N = 20;
        int boosted = 0, plain = 0, audit_ok = 0, snap_ok = 0;
        long boosted_dmg = 0, plain_dmg = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1639, /*mode=*/1);
            boosted += t.crit ? 1 : 0;
            snap_ok += (t.is_crit_snapshot == t.crit) ? 1 : 0;
            audit_ok += (t.audit == (t.crit ? 1 : -1)) ? 1 : 0;
            boosted_dmg += t.damage;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1639, /*mode=*/0);
            plain += t.crit ? 1 : 0;
            snap_ok += (t.is_crit_snapshot == t.crit) ? 1 : 0;
            audit_ok += (t.audit == (t.crit ? 1 : -1)) ? 1 : 0;
            plain_dmg += t.damage;
        }
        std::cout << "  [T5820.a] 1639 boosted crit=" << boosted << "/" << N
                  << " plain crit=" << plain << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (2 * N)
                  << " snap_ok=" << snap_ok << "/" << (2 * N) << std::endl;
        CHECK(boosted == N, "T5820.a 有提升 → 每局必暴（实测 " + std::to_string(boosted) +
                                "/" + std::to_string(N) + "）");
        CHECK(plain == 0, "T5820.a 无提升 → 必暴门闭（实测 " + std::to_string(plain) + "）");
        CHECK(audit_ok == 2 * N, "T5820.a 审计槽逐局吻合（实测 " + std::to_string(audit_ok) +
                                      "/" + std::to_string(2 * N) + "）");
        CHECK(snap_ok == 2 * N, "T5820.a resolvedDamage.isCrit 与旗标一致（实测 " +
                                     std::to_string(snap_ok) + "/" + std::to_string(2 * N) + "）");
        const double ratio = static_cast<double>(boosted_dmg) / static_cast<double>(plain_dmg);
        std::cout << "  [T5820.a] boosted_dmg=" << boosted_dmg << " plain_dmg=" << plain_dmg
                  << " ratio=" << ratio << std::endl;
        CHECK(ratio > 1.7 && ratio < 2.3,
              "T5820.a 必暴伤害 ≈2×（实测 ratio=" + std::to_string(ratio) + "）");
    }

    // ══ T5820.b 1590：低体 → 必暴；等高/高体 → 必不暴 ══
    {
        constexpr int N = 20;
        int low = 0, equal = 0, high = 0, audit_ok = 0;
        long low_dmg = 0, equal_dmg = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1590, /*mode=*/2);
            low += t.crit ? 1 : 0;
            audit_ok += (t.audit == (t.crit ? 1 : -1)) ? 1 : 0;
            low_dmg += t.damage;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1590, /*mode=*/3);
            equal += t.crit ? 1 : 0;
            audit_ok += (t.audit == (t.crit ? 1 : -1)) ? 1 : 0;
            equal_dmg += t.damage;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1590, /*mode=*/4);
            high += t.crit ? 1 : 0;
            audit_ok += (t.audit == (t.crit ? 1 : -1)) ? 1 : 0;
        }
        std::cout << "  [T5820.b] 1590 low crit=" << low << "/" << N
                  << " equal crit=" << equal << "/" << N
                  << " high crit=" << high << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (3 * N) << std::endl;
        CHECK(low == N, "T5820.b 低体 → 每局必暴（实测 " + std::to_string(low) + "/" +
                            std::to_string(N) + "）");
        CHECK(equal == 0, "T5820.b 体力相等 → 门闭（严格小于；实测 " + std::to_string(equal) + "）");
        CHECK(high == 0, "T5820.b 高体 → 门闭（实测 " + std::to_string(high) + "）");
        CHECK(audit_ok == 3 * N, "T5820.b 审计槽逐局吻合（实测 " + std::to_string(audit_ok) +
                                      "/" + std::to_string(3 * N) + "）");
        const double ratio = static_cast<double>(low_dmg) / static_cast<double>(equal_dmg);
        std::cout << "  [T5820.b] low_dmg=" << low_dmg << " equal_dmg=" << equal_dmg
                  << " ratio=" << ratio << std::endl;
        CHECK(ratio > 1.7 && ratio < 2.3,
              "T5820.b 必暴伤害 ≈2×（实测 ratio=" + std::to_string(ratio) + "）");
    }

    std::cout << "scenario_5820 PASS (J251 条件必暴族)" << std::endl;
    return 0;
}
