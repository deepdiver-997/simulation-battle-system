// 场景 5850 —— J254 能力段数固伤 + PP 门威力族（1678/1857，
// moves_lib/j_boost_level_power.cpp）（2026-10-22，worktree feat/jbatch；号段 5850-5859）
//
// 断言口径：
//   - 1678：固定伤害点数由**本体能力等级**直接决定，审计槽 991678100 是精确值；
//     伤害侧证用低攻（atk=1）载体，使技能自身红伤只剩 1~2 点，差值可辨。
//     门只数**正**段且只数槽 2/3（防御/特防）——攻击/速度正段必须不触发。
//   - 1857：威力改写族按坑集 §6 停泊到 BATTLE_FIRST_SKILL_EFFECT 读
//     `ctx.ws.skill_power_view[0].value` 做字段精确断言；门界用 pp==阈值（不触发）
//     与 pp<阈值（触发）两侧夹住。
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
constexpr int k1678  = 36271;   // 倾覆之势 [1678]|[100]（纯链，物 130）
constexpr int k1857  = 36500;   // 迅镰击 [696,1857]|[20,10,50, 50,2]（物 70）

constexpr int k1678Audit = 991678100;
constexpr int k1857Audit = 991857100;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/99999);
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

struct FixedTrial {
    int damage = 0;
    int audit = -999;
};

// def/spdef 为防御/特防槽等级；atk_lv/spd_lv 用于证明"只数防/特防"。
FixedTrial fixed_trial(int def, int spdef, int atk_lv = 0, int spd_lv = 0) {
    ScenarioBattle b(make_party(mk_owner(k1678, /*atk=*/1)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.ability_levels[0][2] = def;    // 防御
    ctx.ability_levels[0][3] = spdef;  // 特防
    ctx.ability_levels[0][0] = atk_lv; // 攻击（不应计入）
    ctx.ability_levels[0][4] = spd_lv; // 速度（不应计入）
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    FixedTrial t;
    t.damage = before - ctx.getPet(1).hp;
    t.audit = audit(ctx, k1678Audit);
    return t;
}

struct FieldTrial {
    int view = -999;
    int audit = -999;
};

// foe_pp < 0 = 不动对手 PP；否则把对手 0 号技能 PP 设为该值。
FieldTrial field_trial(int foe_pp) {
    ScenarioBattle b(make_party(mk_owner(k1857, /*atk=*/300)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (foe_pp >= 0) {
        ctx.getPet(1).skills[0].pp = foe_pp;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    FieldTrial t;
    t.view = ctx.ws.skill_power_view[0].value;
    t.audit = audit(ctx, k1857Audit);
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5850.a 1678：防御+特防正段×100 → 300 固伤；负段/非防特防段不计 ══
    {
        const FixedTrial three = fixed_trial(/*def=*/2, /*spdef=*/1);
        const FixedTrial one_neg = fixed_trial(/*def=*/-2, /*spdef=*/1);
        const FixedTrial wrong_slots = fixed_trial(/*def=*/0, /*spdef=*/0, /*atk_lv=*/2,
                                                   /*spd_lv=*/2);
        const FixedTrial none = fixed_trial(/*def=*/0, /*spdef=*/0);
        std::cout << "  [T5850.a] dmg=" << three.damage << "/" << one_neg.damage << "/"
                  << wrong_slots.damage << "/" << none.damage
                  << " audit=" << three.audit << "/" << one_neg.audit << "/"
                  << wrong_slots.audit << "/" << none.audit << std::endl;
        CHECK_EQ(three.audit, 300, "T5850.a 防御+2 特防+1 → 3 段 ×100 = 300");
        CHECK_EQ(one_neg.audit, 100, "T5850.a 防御-2 特防+1 → 只数正段 = 100");
        CHECK_EQ(wrong_slots.audit, 0, "T5850.a 攻击/速度正段不计入防/特防段数");
        CHECK_EQ(none.audit, 0, "T5850.a 无正段 → 0 固伤");
        CHECK(three.damage >= 301 && three.damage <= 306,
              "T5850.a 300 固伤落地（实测 " + std::to_string(three.damage) + "）");
        CHECK(one_neg.damage >= 101 && one_neg.damage <= 106,
              "T5850.a 100 固伤落地（实测 " + std::to_string(one_neg.damage) + "）");
        CHECK(wrong_slots.damage >= 1 && wrong_slots.damage <= 6,
              "T5850.a 错槽不触发（只剩技能自身 1~5 点，实测 " +
                  std::to_string(wrong_slots.damage) + "）");
        CHECK(none.damage >= 1 && none.damage <= 6,
              "T5850.a 无正段不触发（实测 " + std::to_string(none.damage) + "）");
    }

    // ══ T5850.b 1857：威力 +50%；对手任一技能 PP<2 → 提升翻倍（+100%） ══
    {
        const FieldTrial none = field_trial(/*foe_pp=*/-1);   // 默认满 PP
        const FieldTrial one = field_trial(/*foe_pp=*/1);     // 1 < 2 → 门过
        const FieldTrial zero = field_trial(/*foe_pp=*/0);    // 0 < 2 → 门过
        const FieldTrial boundary = field_trial(/*foe_pp=*/2); // 2 < 2 假 → 门闭
        std::cout << "  [T5850.b] views=" << none.view << "/" << one.view << "/" << zero.view
                  << "/" << boundary.view
                  << " audits=" << none.audit << "/" << one.audit << "/" << zero.audit
                  << "/" << boundary.audit << std::endl;
        CHECK_EQ(none.view, 105, "T5850.b 无低 PP → base70 ×1.5 = 105");
        CHECK_EQ(one.view, 140, "T5850.b 对手 PP=1 <2 → +50% 翻倍 = 140");
        CHECK_EQ(zero.view, 140, "T5850.b 对手 PP=0 <2 → 140");
        CHECK_EQ(boundary.view, 105, "T5850.b 对手 PP=2 不 <2 → 门闭保持 105");
        CHECK_EQ(none.audit, 50, "T5850.b 门闭审计=50");
        CHECK_EQ(one.audit, 1100, "T5850.b 门过审计=1000+有效pct100");
        CHECK_EQ(zero.audit, 1100, "T5850.b PP=0 审计=1100");
        CHECK_EQ(boundary.audit, 50, "T5850.b 边界 PP=2 审计=50");
    }

    std::cout << "scenario_5850 PASS (J254 能力段数固伤+PP门威力族)" << std::endl;
    return 0;
}
