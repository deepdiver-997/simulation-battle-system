// 场景 5860 —— J255 伤害比较器反馈族（632/633/639，
// moves_lib/j_damage_comparator.cpp）（2026-10-22，worktree feat/jbatch；号段 5860-5869）
//
// 断言口径（审计槽 + 绝对量 + 多回合窗口）：
//   - 比较器：0=高于（严格 >）、1=低于（严格 <）。633 用同源双向载体各测一次
//     （33360 [0,300,50] 高伤回血 / 19865 [1,250,50] 低伤回血），把方向码钉死。
//   - 632：审计槽 996320100 = 触发那一发的 final；窗内第 2/3 回合用**无该效果的填充技**
//     拍打，读 `ctx.crit_happened[0]` 绝对布尔；第 4 回合窗过期必须不暴。
//   - 633：回血量按 `heal_amount` 实际恢复断言 = final×pct/100（先压当前 HP 防 clamp）。
//   - 639：审计槽 996390100 = 触发 final；窗内第 2 回合用填充技仍附加 {3} 点固伤，
//     第 3 回合过期不附加（绝对量对比）；996390102 = 窗内附加次数。
//   - 伤害阈值构造按 87~102@atk300/power40 锚点线性缩放：atk=1000 时 power120 的
//     触发技远高于 250/300 阈值，atk=100 时远低于阈值。
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

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充，无被测效果）
constexpr int k632   = 17009;   // 冰魂锁链 [632]|[0,250,2]（纯链，物 120）
constexpr int k633hi = 33360;   // 灵妙之触 [633]|[0,300,50]（纯链，特 130）
constexpr int k633lo = 19865;   // 死亡弥撒 [950,633]|[2, 1,250,50]（物 85）
constexpr int k639   = 16964;   // 万象天明劫 [639]|[0,300,1,300]（纯链，物 160）

constexpr int k632Audit = 996320100;
constexpr int k633Audit = 996330100;
constexpr int k639Audit = 996390100;
constexpr int k639TickAudit = 996390102;

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

// ── 632：首回合载体（高伤触发/低伤门闭），第 2/3/4 回合用填充技看必暴窗口 ──
struct CritTrial {
    bool round1 = false;
    bool round2 = false;
    bool round3 = false;
    bool round4 = false;
    int audit = -999;
};

CritTrial crit_trial(int atk) {
    ScenarioBattle b(make_party(mk_owner(k632, atk)), make_party(mk_foe(1000000)));
    b.start();
    BattleContext& ctx = b.ctx();
    CritTrial t;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.round1 = ctx.crit_happened[0];
    t.audit = audit(ctx, k632Audit);
    for (int r = 0; r < 3; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const bool crit = ctx.crit_happened[0];
        if (r == 0) t.round2 = crit;
        if (r == 1) t.round3 = crit;
        if (r == 2) t.round4 = crit;
    }
    return t;
}

// ── 633：单回合伤害 + 实际回血 ──
struct HealTrial {
    int dealt = 0;
    int healed = 0;
    int audit = -999;
};

HealTrial heal_trial(int carrier, int atk) {
    ScenarioBattle b(make_party(mk_owner(carrier, atk)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = 1000;   // 防回血 clamp
    const int foe_before = ctx.getPet(1).hp;
    const int self_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    HealTrial t;
    t.dealt = foe_before - ctx.getPet(1).hp;
    t.healed = ctx.getPet(0).hp - self_before;
    t.audit = audit(ctx, k633Audit);
    return t;
}

// ── 639：首回合载体（高伤触发/低伤门闭），第 2 回合填充技看 +固伤，第 3 回合过期 ──
struct FixedWindowTrial {
    int dealt2 = 0;
    int dealt3 = 0;
    int audit = -999;
    int ticks = 0;
};

FixedWindowTrial fixed_window_trial(int atk) {
    ScenarioBattle b(make_party(mk_owner(k639, atk)), make_party(mk_foe(1000000)));
    b.start();
    BattleContext& ctx = b.ctx();
    FixedWindowTrial t;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.audit = audit(ctx, k639Audit);
    int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.dealt2 = before - ctx.getPet(1).hp;
    before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.dealt3 = before - ctx.getPet(1).hp;
    t.ticks = audit(ctx, k639TickAudit);
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5860.a 632：高伤触发 → 下 2 回合必暴；低伤门闭 ══
    {
        const CritTrial hit = crit_trial(/*atk=*/1000);
        const CritTrial miss = crit_trial(/*atk=*/100);
        std::cout << "  [T5860.a] hit r1..r4=" << hit.round1 << hit.round2 << hit.round3
                  << hit.round4 << " audit=" << hit.audit
                  << " | miss r1..r4=" << miss.round1 << miss.round2 << miss.round3
                  << miss.round4 << " audit=" << miss.audit << std::endl;
        CHECK(hit.round1 == false, "T5860.a 触发回合本身不追加强暴（R1 非必暴）");
        CHECK(hit.round2 == true, "T5860.a 高伤触发 → R2 必暴");
        CHECK(hit.round3 == true, "T5860.a 高伤触发 → R3 必暴（窗口 2 回合）");
        CHECK(hit.round4 == false, "T5860.a R4 窗过期 → 不暴");
        CHECK(hit.audit > 250, "T5860.a 审计=触发 final>250（实测 " +
                                   std::to_string(hit.audit) + "）");
        CHECK(miss.round1 == false && miss.round2 == false && miss.round3 == false
                  && miss.round4 == false,
              "T5860.a 低伤门闭 → 全程不暴（绝对）");
        CHECK_EQ(miss.audit, -999, "T5860.a 低伤门闭 → 审计未写");
    }

    // ══ T5860.b 633：高伤(>300) 与低伤(<250) 两向都回 final×50% ══
    {
        const HealTrial hi = heal_trial(k633hi, /*atk=*/1000);
        const HealTrial lo = heal_trial(k633lo, /*atk=*/300);
        std::cout << "  [T5860.b] hi dealt=" << hi.dealt << " healed=" << hi.healed
                  << " audit=" << hi.audit
                  << " | lo dealt=" << lo.dealt << " healed=" << lo.healed
                  << " audit=" << lo.audit << std::endl;
        CHECK(hi.dealt > 300, "T5860.b 高伤载体伤害 >300（实测 " + std::to_string(hi.dealt) + "）");
        CHECK_EQ(hi.audit, hi.dealt, "T5860.b 高伤审计=final");
        CHECK_EQ(hi.healed, hi.dealt * 50 / 100, "T5860.b 高伤回血=final×50%");
        CHECK(lo.dealt > 0 && lo.dealt < 250,
              "T5860.b 低伤载体伤害 <250（实测 " + std::to_string(lo.dealt) + "）");
        CHECK_EQ(lo.audit, lo.dealt, "T5860.b 低伤审计=final（方向码 1 生效）");
        CHECK_EQ(lo.healed, lo.dealt * 50 / 100, "T5860.b 低伤回血=final×50%");
    }

    // ══ T5860.c 639：高伤触发 → 下 1 回合用填充技也附加 300 固伤；过期不附加 ══
    {
        const FixedWindowTrial hit = fixed_window_trial(/*atk=*/1000);
        const FixedWindowTrial miss = fixed_window_trial(/*atk=*/100);
        std::cout << "  [T5860.c] hit dealt2=" << hit.dealt2 << " dealt3=" << hit.dealt3
                  << " audit=" << hit.audit << " ticks=" << hit.ticks
                  << " | miss dealt2=" << miss.dealt2 << " dealt3=" << miss.dealt3
                  << " audit=" << miss.audit << " ticks=" << miss.ticks << std::endl;
        CHECK(hit.audit > 300, "T5860.c 审计=触发 final>300（实测 " +
                                   std::to_string(hit.audit) + "）");
        CHECK(hit.dealt2 >= 550, "T5860.c R2 填充技伤害含 300 固伤（实测 " +
                                     std::to_string(hit.dealt2) + "）");
        CHECK(hit.dealt3 <= 450, "T5860.c R3 窗过期不再附带固伤（实测 " +
                                     std::to_string(hit.dealt3) + "）");
        CHECK_EQ(hit.ticks, 1, "T5860.c 窗内恰附加 1 次");
        CHECK_EQ(miss.audit, -999, "T5860.c 低伤门闭 → 审计未写");
        CHECK(miss.dealt2 <= 50, "T5860.c 门闭时 R2 无固伤（实测 " +
                                     std::to_string(miss.dealt2) + "）");
        CHECK_EQ(miss.ticks, -999, "T5860.c 门闭时窗未武装");
    }

    std::cout << "scenario_5860 PASS (J255 伤害比较器反馈族)" << std::endl;
    return 0;
}
