// 场景 7400 —— K387 先后手吸取回复+承伤转护盾族（effect 1618，soul_lib/k387_order_drain_shield.cpp）
//（2026-10-24，worktree ../agents/k387；号段 7400-7409）
//
// 断言口径（K 板规范：hp/护盾带断言——攻击红伤 [87,102] 浮动带，断言带宽容纳）：
//   - ①先手吸取：对手 maxHP/3（99999/3=33333）粉伤 + 印主等量回复（K302 口径：
//     恢复按预取量、不随粉伤减免缩水）。
//   - ①后手：印主 maxHP/3 回复 + 等量 PERCENT_VALUE 粉伤打对手。
//   - ②承伤转护盾：ROUND_END 后读 shield_bank_.total()（ElfPet 字段、不受 ws reset
//     陷阱影响——SOP §五.3）；下回合附伤戳 = 对手掉血里多出的一段。
//   - 印主 hp 压低（SOP §五.2：回满类断言先压血量，1M 血 holders 上 heal 空洞通过）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击载体
constexpr int kMaxHp = 99999;
constexpr int kThird = kMaxHp / 3;   // 33333

// 印主：row1282 本体行（50% 档）。
ElfPet mark_pet_50(int speed, int hp) {
    std::vector<int> args{3, 3, 50, 400, 50, 400, 1};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/hp,
                        /*soul_mark=*/SoulMark(1618, "K387 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主：row1539 伙伴行（100% 档）。
ElfPet mark_pet_100(int speed, int hp) {
    std::vector<int> args{3, 3, 100, 400, 100, 400, 1};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/hp,
                        /*soul_mark=*/SoulMark(1618, "K387 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手：atk=300 → 拍打红伤 raw ∈ [87,102]（常识速查 §五）。
ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7400.a ①先手吸取：印主先手 → 对手 -33333（±自身攻击 1~2），印主 +33333 ══
    {
        ScenarioBattle b(make_party(mark_pet_50(/*speed=*/300, /*hp=*/40000)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4000;   // 压血量（SOP §五.2：heal 观察面防 max 封顶）
        const int foe0 = ctx.getPet(1).hp;
        const int self0 = ctx.getPet(0).hp;
        both_act(b, 0, 0);         // 印主先手拍打；对手拍打
        const int foe_drop = foe0 - ctx.getPet(1).hp;
        CHECK(foe_drop >= kThird && foe_drop <= kThird + 3,
              "T7400.a ①先手：对手掉血 = 吸取33333 + 自身攻击1~2");
        // 印主回复 = 吸取量 33333（4000+33333=37333 < max40000 不封顶），
        // 净变化 = 33333 - 对手攻击(87~102)
        const int self_delta = ctx.getPet(0).hp - self0;
        CHECK(self_delta >= kThird - 105 && self_delta <= kThird - 85,
              "T7400.a ①先手：印主净回血 = 33333 - 对手攻击(87~102)");
    }

    // ══ T7400.b ①后手：印主后手 → 回复 maxHP/3=13333 + 等量粉伤打对手 ══
    {
        ScenarioBattle b(make_party(mark_pet_50(/*speed=*/100, /*hp=*/40000)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4000;   // 压血量：4000+13333=17333 < 40000 不封顶
        const int foe0 = ctx.getPet(1).hp;
        const int self0 = ctx.getPet(0).hp;
        both_act(b, 0, 0);         // 对手先手拍打；印主后手拍打
        const int foe_drop = foe0 - ctx.getPet(1).hp;
        // 对手掉血 = 印主攻击(1~2) + 等量粉伤 13333
        CHECK(foe_drop >= 13333 && foe_drop <= 13333 + 3,
              "T7400.b ①后手：对手掉血 = 等量百分比粉伤13333 + 自身攻击1~2");
        const int self_delta = ctx.getPet(0).hp - self0;
        CHECK(self_delta >= 13333 - 105 && self_delta <= 13333 - 85,
              "T7400.b ①后手：印主净回血 = 13333 - 对手攻击(87~102)");
    }

    // ══ T7400.c ②承伤转护盾（50% 档）：R1 末盾 = 所受×50%（87~102 → 43~51）══
    {
        ScenarioBattle b(make_party(mark_pet_50(/*speed=*/300, /*hp=*/99999)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0, 0);
        const int shield = ctx.getPet(0).shield_bank_.total();
        CHECK(shield >= 43 && shield <= 51,
              "T7400.c ②：R1 末盾 = 所受(87~102)×50% ∈ [43,51]");
        // R2：印主先手攻击 → ①吸取 33333 + ②附伤戳（43~51）+ 自身攻击 1~2
        const int foe0 = ctx.getPet(1).hp;
        both_act(b, 0, 0);
        const int foe_drop = foe0 - ctx.getPet(1).hp;
        CHECK(foe_drop >= kThird + 43 && foe_drop <= kThird + 53 + 3,
              "T7400.c ②：R2 对手掉血 = 吸取33333 + 附伤戳(43~51) + 攻击1~2");
    }

    // ══ T7400.d ②伙伴行 100% 档：R1 末盾 = 所受×100% ∈ [87,102] ══
    {
        ScenarioBattle b(make_party(mark_pet_100(/*speed=*/300, /*hp=*/99999)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0, 0);
        const int shield = ctx.getPet(0).shield_bank_.total();
        CHECK(shield >= 87 && shield <= 102,
              "T7400.d ②：伙伴行 100% → 盾 = 所受 ∈ [87,102]");
    }

    // ══ T7400.e 无承伤回合：无盾无戳（对手 NONE，印主先手仍吸取①）══
    {
        ScenarioBattle b(make_party(mark_pet_50(/*speed=*/300, /*hp=*/99999)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                 "T7400.e ②：无承伤 → ROUND_END 不产盾");
        // ①先手吸取照常（对手 NONE 也是"印主先出手用技"）
        const int foe_drop = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_drop >= kThird && foe_drop <= kThird + 3,
              "T7400.e ①：对手 NONE 回合吸取照常");
    }

    std::cout << "scenario_7400_order_drain_shield PASS" << std::endl;
    return 0;
}
