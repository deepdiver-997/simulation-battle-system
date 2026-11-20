// 场景 4000 —— K250 攻击/命中门回合末固定伤+回复族（1536，soul_lib/k250_usedhit_roundend_attach_heal_1536.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 4000-4009 → T4000 一块四例）
//
// 断言口径（附加/回复为 max 公式精确值；低体门在 ROUND_END 判当前血）：
//   - a 高体攻击（双旗）：回复恰 33333（预压血量防封顶）+ 对手掉血 = 红伤带+33333。
//   - b 低体攻击：翻倍 → 回复恰 66666 + 对手掉血 = 红伤带+66666。
//   - c 双 NONE：无旗无附加无回复（原地）。
//   - d 属性技：used_attribute 门 → 使用旗不置 → 无附加无回复。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kGaoSu = 20017;  // 高速移动：属性技 filler（2830 实证）

SoulMark make_mark() {
    SoulMark sm(1536, "K250 魂印", "test", EffectArgs(std::vector<int>{3, 2, 3, 2, 1}));
    sm.monster_id = 3938;   // 圣骑·赫里斯
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa},
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

void one_round(ScenarioBattle& b, BattleFsm::ActionType a0, int idx) {
    b.submit_operation(0, a0, idx >= 0 ? idx : -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4000.a 高体攻击：附加 33333 + 回复 33333 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 66000;   // 高体（>max/2）且回复余量足
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 99999 / 3, "T4000.a 回复 = max/3");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 33415 && foe_delta < 33440,
              "T4000.a 对手掉血 = 红伤带 + 附加 33333");
        std::cout << "  [T4000.a] 高体双触发 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T4000.b 低体攻击：翻倍 66666 + 66666 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 4000;   // < max/2（ROUND_END 判当前血）
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 99999 / 3 * 2, "T4000.b 低体回复翻倍 = max/3×2");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 66745 && foe_delta < 66775,
              "T4000.b 对手掉血 = 红伤带 + 附加 66666");
        std::cout << "  [T4000.b] 低体翻倍 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T4000.c 双 NONE：静默 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 4000;   // 即使低体也无旗
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T4000.c 无攻击不回复");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T4000.c 无攻击不附加");
        std::cout << "  [T4000.c] 无攻击静默 通过\n";
    }

    // ══ T4000.d 属性技：used_attribute 门 → 静默 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 4000;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T4000.d 属性技不回复");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T4000.d 属性技不附加");
        std::cout << "  [T4000.d] 属性技静默 通过\n";
    }

    std::cout << "scenario_4000 PASS (K250 攻击/命中门回合末固定伤+回复族)" << std::endl;
    return 0;
}
