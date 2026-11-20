// 场景 3830 —— K239 必发烧伤+吸伤回血+异常门必暴族（1899，soul_lib/k239_burn_drain_anomaly_mustcrit_1899.cpp）
//（2026-10-12，夜跑 k-prod-6；号段 3830-3839 → T3830 一块四例）
//
// 断言口径（100% 档申报制 → 全确定性）：
//   - a 命中 → 烧伤落地（100% 档短路，K226 口径）。
//   - b 已烧 → 吸伤回血守恒等式：预铺对手烧伤，己方攻击红伤 R →
//     自身回复恰 R（自/他 HP 差相等的守恒断言，零浮动依赖）。
//   - c 未烧 → 不回血（首击只上烧伤，自身原地）。
//   - d 异常门必暴：预铺麻痹（无 tick）+ 对手烧伤免疫（防子句 A tick 污染带）
//     → R1 常规带 [62,99] → ROUND_END 异常门 → R2 必暴带（>115）。
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark() {
    SoulMark sm(1899, "K239 魂印", "test", EffectArgs(std::vector<int>{100, 100, 1}));
    sm.monster_id = 4313;   // 灼日之蚀
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
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

    // ══ T3830.a 命中 → 烧伤 100% 落地 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T3830.a 攻击命中 → 烧伤落地");
        std::cout << "  [T3830.a] 烧伤落地 通过\n";
    }

    // ══ T3830.b 已烧 → 吸伤回血守恒等式 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 1, kBurn, 3, 0), ApplyAnomalyResult::SUCCESS,
                 "T3830.b 预铺对手烧伤");
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // 回复余量
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int self_gain = ctx.getPet(0).hp - hp0;
        const int raw = ctx.ws.raw_attack_damage[0];   // 裸伤台账（实现读数同源）
        CHECK(raw > 0, "T3830.b raw_attack_damage 台账可读");
        CHECK_EQ(self_gain, raw, "T3830.b 吸伤回血 = raw×100%");
        std::cout << "  [T3830.b] 吸伤回血 通过（R=" << raw << "）\n";
    }

    // ══ T3830.c 未烧 → 不回血 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3830.c 未烧不回血");
        std::cout << "  [T3830.c] 未烧静默 通过\n";
    }

    // ══ T3820.d 异常门必暴：预铺麻痹（无 tick）+ 烧伤免疫（子句 A 静默防 tick 污染）
    //    R1 常规带 → ROUND_END 对手有异常 → R2 必暴带 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           1u << kBurn, 0, 0, false, EffectScope::ON_STAGE);
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis),
                               5, 0),
                 ApplyAnomalyResult::SUCCESS, "T3830.d 预铺麻痹（异常门源）");
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int d1 = foe_hp0 - ctx.getPet(1).hp;
        CHECK(d1 > 85 && d1 < 104, "T3830.d R1 常规红伤（无必暴）");
        const int foe_hp1 = ctx.getPet(1).hp;
        one_round(b);
        const int d2 = foe_hp1 - ctx.getPet(1).hp;
        CHECK(d2 > 115, "T3830.d R2 必暴伤害跃出常规带");
        std::cout << "  [T3830.d] 异常门必暴 通过（d1=" << d1 << " d2=" << d2 << "）\n";
    }

    std::cout << "scenario_3830 PASS (K239 必发烧伤+吸伤回血+异常门必暴族)" << std::endl;
    return 0;
}
