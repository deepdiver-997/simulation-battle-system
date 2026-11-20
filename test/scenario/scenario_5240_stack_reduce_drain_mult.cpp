// 场景 5240 —— K351 叠减伤/吸血与概率倍增族（1059/1060，
// soul_lib/k_stack_reduce_drain_mult.cpp）
//（2026-10-22，worktree k351；号段 5240-5249 → T5240-T5243 四块）
//
// 断言口径（红伤带锚 atk300/def100 raw ∈ [87,102] → [85,104]）：
//   - T5240 1059 叠减伤：R1/R2/R3/R4 减伤 35/50/65/65 → 受伤带
//     [54,68]/[41,53]/[28,37]/[28,37]；同时白盒断言 ws.damage_reduce_add[0]
//     首槽 = 35/50/65/65（精确）。
//   - T5241 1059 吸血：非暴击 heal == floor(final/2)；暴击（crit 16/16）
//     heal == final（100%），两条都用"回复量 == 对手掉血量的比例"精确断言。
//   - T5242 1060 五倍伤害：概率 100 档 → 对手掉血 ∈ [425,520]（≈5×[85,104]）；
//     回合末回复 50+50×6=350（精确）。
//   - T5243 1060 零概率对照：概率 0 档 → 掉血 ∈ [85,104]，回复仍 350。
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kHp = 200000;
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;

SoulMark make_mark(int id, std::vector<int> args) {
    SoulMark sm(id, "K351 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = (id == 1059) ? 3460 : 3459;
    return sm;
}

ElfPet mark_pet(int id, std::vector<int> args, int atk) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kHp, make_mark(id, std::move(args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet attacker_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// owner 不出手（NONE），对手攻击；返回 owner 掉血量。
long long take_hit_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.getPet(0).hp;
}

// owner 攻击，对手 NONE；返回对手掉血量。
long long deal_hit_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

int reduce_slot0(const BattleContext& ctx) {
    return ctx.ws.damage_reduce_add[0][0];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5240 1059 叠减伤：35/50/65/65（白盒 + 受伤带）════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1059, {35, 15, 65, 50, 100}, 1)),
                         make_party(attacker_pet()));
        b.start();
        const int expect_reduce[4] = {35, 50, 65, 65};
        const int lo[4] = {54, 41, 28, 28};
        const int hi[4] = {68, 53, 37, 37};
        for (int r = 0; r < 4; ++r) {
            const long long dmg = take_hit_round(b);
            CHECK_EQ(reduce_slot0(b.ctx()), expect_reduce[r],
                     "T5240 1059：R" + std::to_string(r + 1) +
                         " 减伤槽精确值（ws.damage_reduce_add[0][0]）");
            CHECK(dmg >= lo[r] && dmg <= hi[r],
                  "T5240 1059：R" + std::to_string(r + 1) + " 受伤带 [" +
                      std::to_string(lo[r]) + "," + std::to_string(hi[r]) +
                      "]（实测 " + std::to_string(dmg) + "）");
        }
        std::cout << "  [T5240] 1059 叠减伤 通过\n";
    }

    // ══ T5241 1059 吸血：50%（暴击 100%）══════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1059, {35, 15, 65, 50, 100}, 300)),
                         make_party(attacker_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= 1000;   // 压血
        const long long foe_dmg = deal_hit_round(b);
        const long long heal = ctx.getPet(0).hp - (kHp - 1000);
        CHECK(heal == foe_dmg / 2,
              "T5241 1059：非暴击吸血 == floor(final/2)（heal=" +
                  std::to_string(heal) + " dmg=" + std::to_string(foe_dmg) + "）");
        std::cout << "  [T5241a] 1059 非暴击吸血 通过（heal=" << heal
                  << " dmg=" << foe_dmg << "）\n";
    }
    {
        ElfPet crit = mark_pet(1059, {35, 15, 65, 50, 100}, 300);
        set_crit_sixteenths(crit, 16);   // 必定暴击
        ScenarioBattle b(make_party(crit), make_party(attacker_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= 1000;
        const long long foe_dmg = deal_hit_round(b);
        const long long heal = ctx.getPet(0).hp - (kHp - 1000);
        CHECK(foe_dmg >= 2 * kRedLo,
              "T5241 1059：暴击红伤 ≈×2（实测 " + std::to_string(foe_dmg) + "）");
        CHECK(heal == foe_dmg,
              "T5241 1059：暴击吸血 == final（heal=" + std::to_string(heal) +
                  " dmg=" + std::to_string(foe_dmg) + "）");
        std::cout << "  [T5241b] 1059 暴击吸血 通过（heal=" << heal
                  << " dmg=" << foe_dmg << "）\n";
    }

    // ══ T5242 1060 五倍伤害（概率 100 档）+ 回合末回复 350 ════════════════════
    {
        ScenarioBattle b(
            make_party(mark_pet(1060, {100, 5, 4, 50, 50, 1}, 300)),
            make_party(attacker_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= 1000;
        const long long foe_dmg = deal_hit_round(b);
        const long long heal = ctx.getPet(0).hp - (kHp - 1000);
        CHECK(foe_dmg >= 425 && foe_dmg <= 520,
              "T5242 1060：概率 100 档五倍伤害 ∈ [425,520]（实测 " +
                  std::to_string(foe_dmg) + "）");
        CHECK_EQ(heal, 350,
                 "T5242 1060：回合末回复 50+50×6=350（实测 " +
                     std::to_string(heal) + "）");
        std::cout << "  [T5242] 1060 五倍伤害 + 团队回复 通过（dmg=" << foe_dmg
                  << " heal=" << heal << "）\n";
    }

    // ══ T5243 1060 零概率对照（base=0/per_foe=0）→ 普通红伤 ══════════════════
    {
        ScenarioBattle b(
            make_party(mark_pet(1060, {0, 5, 0, 50, 50, 1}, 300)),
            make_party(attacker_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= 1000;
        const long long foe_dmg = deal_hit_round(b);
        const long long heal = ctx.getPet(0).hp - (kHp - 1000);
        CHECK(foe_dmg >= kRedLo && foe_dmg <= kRedHi,
              "T5243 1060c：零概率对照普通红伤（实测 " +
                  std::to_string(foe_dmg) + "）");
        CHECK_EQ(heal, 350,
                 "T5243 1060c：回复与概率无关仍 350（实测 " +
                     std::to_string(heal) + "）");
        std::cout << "  [T5243] 1060 零概率对照 通过（dmg=" << foe_dmg
                  << " heal=" << heal << "）\n";
    }

    std::cout << "scenario_5240 PASS (K351 叠减伤/吸血与概率倍增族)" << std::endl;
    return 0;
}
