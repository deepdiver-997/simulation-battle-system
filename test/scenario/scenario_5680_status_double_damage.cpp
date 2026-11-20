// 场景 5680 —— J237 状态门双倍伤害族（193，moves_lib/j_status_double_damage.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5680-5689）
//
// 断言口径（精确快照字段 + 伤害比值双保险）：
//   - 观测面 = 先手行动结算后的 `ctx.resolvedDamage`（attackerId/defenderId/isRed 自卡），
//     不测回合末 hp 差——避免异常自身回合末扣血污染（《场景断言口径坑集》§5）。
//   - 193 的落点是 ws.damage_add_pct += 100 → 快照 `addPct` **精确 == 100**（未触发 == 0）。
//     这是无随机精确断言；伤害比值（≈2×）作行为侧证。
//   - T5680.a：14858 狂枭粉碎击 [193]|[7]，对手疲惫(7) → addPct=100，dmg≈2×。
//   - T5680.b：同载体，对手害怕(6)（非指定异常）→ addPct=0，dmg≈1×（状态门精确）。
//   - T5680.c：13154 冰怒刺拳 [193]|[5]，对手冻伤(5) → addPct=100（异常码逐载体）。
//   - T5680.d：16165 秘魂斩 [193,193,193]|[1,2,5]，对手同时中毒+烧伤+冻伤 →
//     addPct=100（多实例去重，不是 300=×4）；dmg 比值 < 2.8 佐证。
//   - 载体 accuracy 统一 100 + must_hit，排除 miss；atk/def/HP 同基线，只有异常槽不同。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/battleWorkspace.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa      = 10004;   // 拍打：物 40（填充刀）
constexpr int kGateFatigue = 14858;  // [193]|[7] 狂枭粉碎击：物 150
constexpr int kGateFrost   = 13154;  // [193]|[5] 冰怒刺拳：物 100
constexpr int kMulti       = 16165;  // [193,193,193]|[1,2,5] 秘魂斩：物 85
constexpr int kMaxHp       = 99999;

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

struct AttackResult {
    int final_damage = 0;
    int add_pct = 0;
};

// 先手行动结算后停泊读快照（异常状态在提交操作前注入回合数组）。
AttackResult first_attack_snapshot(int carrier, const std::vector<int>& anomalies) {
    ScenarioBattle b(make_party(mk_self(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    for (const int status : anomalies) {
        ctx.apply_abnormal_status_for_rounds(1, status, 3);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
    ctx.debug_step_mode = false;

    const DamageSnapshot& d = ctx.resolvedDamage;
    CHECK(d.attackerId == 0 && d.defenderId == 1 && d.isRed,
          "T5680 观测面为本次先手红伤快照");
    AttackResult r;
    r.final_damage = d.final;
    r.add_pct = d.addPct;
    return r;
}

constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);   // 7
constexpr int kFear    = static_cast<int>(AbnormalStatusId::Fear);      // 6
constexpr int kFrost   = static_cast<int>(AbnormalStatusId::Frostbite); // 5
constexpr int kPoison  = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kBurn    = static_cast<int>(AbnormalStatusId::Burn);      // 2

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5680.a 疲惫门：对手疲惫 → ×2 ══
    {
        const AttackResult base = first_attack_snapshot(kGateFatigue, {});
        const AttackResult cond = first_attack_snapshot(kGateFatigue, {kFatigue});
        std::cout << "  [T5680.a] 14858 fatigue gate: base dmg=" << base.final_damage
                  << " addPct=" << base.add_pct << " | cond dmg=" << cond.final_damage
                  << " addPct=" << cond.add_pct << std::endl;
        CHECK_EQ(base.add_pct, 0, "T5680.a 无异常 → addPct=0");
        CHECK_EQ(cond.add_pct, 100, "T5680.a 对手疲惫(7) → addPct=100（双倍）");
        CHECK(cond.final_damage > base.final_damage * 15 / 10,
              "T5680.a 疲惫门伤害 ≈2×（实测 cond=" + std::to_string(cond.final_damage) +
                  " base=" + std::to_string(base.final_damage) + "）");
    }

    // ══ T5680.b 状态门精确：害怕(6) 不是 {0}=7 → 不触发 ══
    {
        const AttackResult base = first_attack_snapshot(kGateFatigue, {});
        const AttackResult wrong = first_attack_snapshot(kGateFatigue, {kFear});
        std::cout << "  [T5680.b] 14858 wrong status: base dmg=" << base.final_damage
                  << " | fear dmg=" << wrong.final_damage
                  << " addPct=" << wrong.add_pct << std::endl;
        CHECK_EQ(wrong.add_pct, 0, "T5680.b 对手害怕(6) ≠ {0}=7 → addPct=0");
        CHECK(wrong.final_damage < base.final_damage * 14 / 10,
              "T5680.b 错误状态不增伤（实测 fear=" + std::to_string(wrong.final_damage) +
                  " base=" + std::to_string(base.final_damage) + "）");
    }

    // ══ T5680.c 冻伤门：13154 {0}=5 ══
    {
        const AttackResult base = first_attack_snapshot(kGateFrost, {});
        const AttackResult cond = first_attack_snapshot(kGateFrost, {kFrost});
        std::cout << "  [T5680.c] 13154 frost gate: base dmg=" << base.final_damage
                  << " | cond dmg=" << cond.final_damage
                  << " addPct=" << cond.add_pct << std::endl;
        CHECK_EQ(cond.add_pct, 100, "T5680.c 对手冻伤(5) → addPct=100");
        CHECK(cond.final_damage > base.final_damage * 15 / 10,
              "T5680.c 冻伤门伤害 ≈2×（实测 cond=" + std::to_string(cond.final_damage) +
                  " base=" + std::to_string(base.final_damage) + "）");
    }

    // ══ T5680.d 多实例去重：16165 三个 193 实例只翻一次 ══
    {
        const AttackResult base = first_attack_snapshot(kMulti, {});
        const AttackResult multi = first_attack_snapshot(kMulti, {kPoison, kBurn, kFrost});
        std::cout << "  [T5680.d] 16165 multi-instance: base dmg=" << base.final_damage
                  << " addPct=" << base.add_pct << " | multi dmg=" << multi.final_damage
                  << " addPct=" << multi.add_pct << std::endl;
        CHECK_EQ(base.add_pct, 0, "T5680.d 无异常 → addPct=0");
        CHECK_EQ(multi.add_pct, 100,
                 "T5680.d 三实例同链只翻一次 → addPct=100（非 300=×4）");
        CHECK(multi.final_damage < base.final_damage * 28 / 10,
              "T5680.d 去重后伤害不落到 ×4 带（实测 multi=" +
                  std::to_string(multi.final_damage) + " base=" +
                  std::to_string(base.final_damage) + "）");
    }

    std::cout << "scenario_5680 PASS (193 状态门双倍伤害 + 多实例去重)" << std::endl;
    return 0;
}
