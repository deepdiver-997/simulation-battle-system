// 场景 3820 —— K238 满体受击害怕/消除回合类族（883，soul_lib/k238_fullhp_hit_fear_break_883.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3820-3829 → T3820 一块四例）
//
// 断言口径（害怕 100% 申报制 → 全确定性）：
//   - a 满体受击 → 害怕落地（对手 Fear 活跃；100% 档确定性，K226 T.b 口径）。
//   - b 恐惧被免疫 → 当场断对手回合类：对手预挂 3 回合封攻（grant_skill_invalid
//     内联入口造回合类观察量）→ R1 受击（怕被免）→ R2 对手攻击**落地**（封已被断，
//     印主掉血）；无印对照局封不受断 → R2 攻击仍被无效（不掉血）。
//   - c 旗未消费兜底：R1 无攻击发生（双方 NONE）→ ROUND_END 断对手回合类 →
//     R2 对手攻击落地（同 b 观察量）；非满体负例旗不置 → R2 仍被封（不掉血）。
//   - d 非满体受击：无旗无害怕（对手 Fear 不活跃）。
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
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);

SoulMark make_mark() {
    SoulMark sm(883, "K238 魂印", "test", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 3270;
    return sm;
}

ElfPet mark_pet(int hp) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    if (hp > 0) {
        p.hp = hp;
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

// 无魂印对照宠（b/c 的"封不被断"对照）。
ElfPet plain_pet(int hp) {
    ElfPet p = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    if (hp > 0) {
        p.hp = hp;
    }
    return p;
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

    // ══ T3820.a 满体受击 → 害怕落地 ══
    {
        ScenarioBattle b(make_party(mark_pet(-1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T3820.a 满体受击 → 害怕 100% 落地");
        std::cout << "  [T3820.a] 满体受击害怕 通过\n";
    }

    // ══ T3820.b 恐惧被免疫 → 当场断对手回合类（对照局不被断）══
    // 观察量 = 对手名下回合类封属（break 的 clear_round_type(target) 只清**施放方=
    // target** 名下的封——"封属属于其施放方"，故种 source=1 的自封做断言夹具）。
    {
        ScenarioBattle b(make_party(mark_pet(-1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           1u << kFear, 0, 0, false, EffectScope::ON_STAGE);
        ctx.grant_skill_invalid(1, 1, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
        CHECK(ctx.rule_center_.has_round_type(1), "T3820.b 夹具：对手名下有回合类封");
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);   // 受击（怕被免）→ 当场断
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T3820.b 害怕被免疫未落地");
        CHECK(!ctx.rule_center_.has_round_type(1), "T3820.b 未触发 → 对手回合类被断");
        std::cout << "  [T3820.b] 免怕断回合类 通过\n";
    }
    {
        ScenarioBattle b(make_party(plain_pet(-1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_skill_invalid(1, 1, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(ctx.rule_center_.has_round_type(1), "T3820.b 对照：无印 → 对手回合类仍在");
        std::cout << "  [T3820.b] 对照局 通过\n";
    }

    // ══ T3820.c 旗未消费兜底（满体无人打 → 断）+ 非满体负例 ══
    {
        ScenarioBattle b(make_party(mark_pet(-1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_skill_invalid(1, 1, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
        one_round(b, BattleFsm::ActionType::NONE);   // 无攻击 → ROUND_END 兜底断
        CHECK(!ctx.rule_center_.has_round_type(1), "T3820.c 兜底：对手回合类被断");
        std::cout << "  [T3820.c] 兜底断回合类 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(99500)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_skill_invalid(1, 1, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
        one_round(b, BattleFsm::ActionType::NONE);   // 非满体 → 旗不置 → 不兜底
        CHECK(ctx.rule_center_.has_round_type(1), "T3820.c 非满体负例：不触发不断");
        std::cout << "  [T3820.c] 非满体负例 通过\n";
    }

    // ══ T3820.d 非满体受击：无害怕 ══
    {
        ScenarioBattle b(make_party(mark_pet(99500)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T3820.d 非满体受击不触发");
        std::cout << "  [T3820.d] 非满体静默 通过\n";
    }

    std::cout << "scenario_3820 PASS (K238 满体受击害怕/消除回合类族)" << std::endl;
    return 0;
}
