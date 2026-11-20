// 场景 4020 —— K252 爬坡毒+回合类门减伤+提升门增伤族（1177，soul_lib/k252_climb_poison_roundgate_boost.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 4020-4029 → T4020 一块四例）
//
// 断言口径（审计槽梯度精确 + 带分离）：
//   - a 爬坡毒逐次爬：连续 4 次攻击使用 → 申报审计 40/50/60/70（每次使用 +10，
//     与 662"未触发才爬"不同源——逐次爬）。中毒落地不硬断言。
//   - b 回合类门减伤 40%：自封夹具（grant_skill_invalid(0,0)）→ 受击带 [37,59]
//     （=红伤带×0.6，与对照 [62,99] 分离）；无夹具对照局常规带。
//   - c 提升门增伤 40%：直铺攻击+1 → d > 102（×1.4 下界 1.19B > 常规上界 99）。
//   - c 内负例：无提升 → 常规带 [62,99]。
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
constexpr int kPoisonAuditBase = 991454000;    // +owner：本次申报%（实现文件同锚）

SoulMark make_mark() {
    SoulMark sm(1177, "K252 魂印", "test", EffectArgs(std::vector<int>{40, 10, 80, 40, 40, 1}));
    sm.monster_id = 3595;   // 泰拉图斯
    return sm;
}

ElfPet mark_pet(int atk) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, atk, 99999, make_mark());
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

void one_round(ScenarioBattle& b, BattleFsm::ActionType a0, BattleFsm::ActionType a1) {
    b.submit_operation(0, a0, a0 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.submit_operation(1, a1, a1 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4020.a 爬坡毒逐次爬：申报 40/50/60/70 ══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect[4] = {40, 50, 60, 70};
        for (int round = 1; round <= 4; ++round) {
            one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
            CHECK_EQ(audit_int(ctx, kPoisonAuditBase + 0), expect[round - 1],
                     "T4020.a 爬坡梯度逐次 +10");
        }
        std::cout << "  [T4020.a] 爬坡毒 40/50/60/70 通过\n";
    }

    // ══ T4020.b 回合类门减伤 40%（自封夹具 + 对照）══
    {
        ScenarioBattle b(make_party(mark_pet(1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_skill_invalid(0, 0, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d = hp0 - ctx.getPet(0).hp;
        CHECK(d > 50 && d < 63, "T4020.b 有回合类 → 减伤 40%（raw[87,102]×0.6=[52,61]）");
        std::cout << "  [T4020.b] 回合类门减伤 通过（d=" << d << "）\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d = hp0 - ctx.getPet(0).hp;
        CHECK(d > 85 && d < 104, "T4020.b 对照：无回合类 → 常规带（raw [87,102]）");
        std::cout << "  [T4020.b] 对照局 通过（d=" << d << "）\n";
    }

    // ══ T4020.c 提升门增伤 40%（含无提升负例）：对手挂毒免疫——防毒 tick 污染掉血带
    //    （爬坡申报审计不受免疫影响）══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           1u << static_cast<int>(AbnormalStatusId::Poison),
                           0, 0, false, EffectScope::ON_STAGE);
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        const int d_noboost = foe_hp0 - ctx.getPet(1).hp;
        std::cerr << "[diag] d_noboost=" << d_noboost << std::endl;
        CHECK(d_noboost > 85 && d_noboost < 104, "T4020.c 无提升 → 常规威力");
        ctx.ability_levels[0][0] = 1;   // 直铺攻击提升（k172 判据门）
        const int foe_hp1 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        const int d_boost = foe_hp1 - ctx.getPet(1).hp;
        CHECK(d_boost > 102, "T4020.c 有提升 → 增伤 40%（×1.4 下界 > 常规上界）");
        std::cout << "  [T4020.c] 提升门增伤 通过（" << d_noboost << "→" << d_boost << "）\n";
    }

    std::cout << "scenario_4020 PASS (K252 爬坡毒+回合类门减伤+提升门增伤族)" << std::endl;
    return 0;
}
