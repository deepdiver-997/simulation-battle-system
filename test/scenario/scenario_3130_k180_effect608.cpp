// 场景 3130 —— K180 免疫低伤+反弹族（608，soul_lib/k_lowhit_immune_reflect.cpp）
//（2026-10-07，worktree ../agents/k180；号段 3130-3139 → T3130-T3132 三块）
//
// 断言口径（真实攻击走伤害管线；85%~100% 浮动不影响比例关系式）：
//   - T3130 低伤免疫且不反弹：atk=1 拍打（伤 1~2 < 300）→ 印主 hp 不动 +
//     对手 hp 不动（独立子句口径：被免=受到 0 → 无反弹）；
//   - T3131 行1 [300,3,1] 高伤受伤+反弹 1/3：atk=5000 拍打 →
//     opp_loss * 3 == self_loss（int 整除精确等式，浮动无关）；
//   - T3132 行2 [300,2,1] 伙伴行分母 2：opp_loss * 2 == self_loss。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物理攻击 filler
constexpr int kMaxHp = 99999;

ElfPet mark_pet(const std::vector<int>& row_args, int atk) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, atk, kMaxHp,
                        /*soul_mark=*/SoulMark(608, "K180 魂印", "",
                                               EffectArgs(
                                                   std::vector<int>{row_args[0],
                                                                    row_args[1],
                                                                    row_args[2]})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, atk, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主 NONE 先手、对手拍打后手打印主；返回 {self_loss, opp_loss}
std::pair<int, int> foe_strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    BattleContext& ctx = b.ctx();
    return {kMaxHp - ctx.getPet(0).hp, kMaxHp - ctx.getPet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3130 行1 低伤免疫且不反弹（atk=1 → 伤 1~2 < 300）══
    {
        ScenarioBattle b(make_party(mark_pet({300, 3, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK_EQ(self_loss, 0, "T3130 低伤被免疫（印主 hp 不动）");
        CHECK_EQ(opp_loss, 0, "T3130 被免不反弹（对手 hp 不动，独立子句口径）");
        std::cout << "  [T3130] 608 低伤免疫+不反弹 通过\n";
    }

    // ══ T3131 行1 [300,3,1] 高伤受伤 + 反弹 1/3 ══
    {
        ScenarioBattle b(make_party(mark_pet({300, 3, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK(self_loss > 300,
              "T3130 高伤落地前提不成立：self_loss=" << self_loss);
        CHECK(opp_loss > 0, "T3131 高伤应触发反弹");
        CHECK_EQ(opp_loss, self_loss / 3,
                 "T3131 反弹 1/3 比例失配（int 整除）：self="
                     << self_loss << " opp=" << opp_loss);
        std::cout << "  [T3131] 608 反弹 1/3（self=" << self_loss
                  << " opp=" << opp_loss << "）通过\n";
    }

    // ══ T3132 行2 [300,2,1] 伙伴行反弹 1/2 ══
    {
        ScenarioBattle b(make_party(mark_pet({300, 2, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK(self_loss > 300,
              "T3132 高伤落地前提不成立：self_loss=" << self_loss);
        CHECK_EQ(opp_loss, self_loss / 2,
                 "T3132 反弹 1/2 比例失配（int 整除）：self="
                     << self_loss << " opp=" << opp_loss);
        std::cout << "  [T3132] 608 伙伴行反弹 1/2（self=" << self_loss
                  << " opp=" << opp_loss << "）通过\n";
    }

    std::cout << "scenario_3130 PASS (K180 免疫低伤+反弹族)" << std::endl;
    return 0;
}
