// 场景 1770 —— J87 盾破反噬/暴击吸取族（2 宏，moves_lib/j_shield_break_fixed.cpp）
//（2026-10-03，worktree ../agents/j87；号段 1770-1779 → T1770-T1771 两块）
//
// 断言口径：
//   - 1387（悲剧终幕 [300,300] 纯链）：R1 获盾+武装；R2 对手大攻击（atk=400，一击
//     破盾+溢出）→ 盾破反噬 fixed 300 给对手（对手掉血 ≥300）；R3 再被击 → 无二次
//     反噬（一次性旗）且护盾已空（我方继续掉血）；
//   - 1659（细雪迷离眼 [1484✓,1659]|[200,300,200,300]，1484=J53 已验良性）：必暴档
//     → 转吸取（对手掉血 ∈ [200,306] 吸区间+红伤；自身回 = dealt ∈ [200,300]）；
//     零暴对照 → 固伤分支（对手掉血 ∈ [200,306]、自身不回）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kBeiJu  = 37171;  // 悲剧终幕 [1387]|[300,300]：物140（盾破反噬载体）
constexpr int kXiXue  = 35252;  // 细雪迷离眼 [1484,1659]|[200,300,200,300]：物（暴击吸取载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, 100, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1770 1387 盾破反噬（悲剧终幕 [300,300]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kBeiJu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 获盾
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // R2：对手大攻击（~480>300 盾量）一击破盾+溢出穿血 → 反噬 300
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = 99999 - ctx.getPet(1).hp;
        CHECK(foe_loss >= 300,
              "T1770.a 1387：盾破 → 反噬 300 固伤（对手累计掉 " +
                  std::to_string(foe_loss) + "）");
        CHECK(ctx.getPet(0).hp < kMaxHp,
              "T1770.a 前置：护盾已破、溢出穿血（我方血 " +
                  std::to_string(ctx.getPet(0).hp) + "）");
        const int self_hp_r2 = ctx.getPet(0).hp;
        // R3：再被击 → 无二次反噬（一次性），护盾已空继续掉血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < self_hp_r2,
              "T1770.b 1387：盾空后再击继续掉血（" + std::to_string(self_hp_r2) + "→" +
                  std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(99999 - ctx.getPet(1).hp - foe_loss <= 60,
              "T1770.b 1387：无二次反噬（对手本轮仅吃我方拍打，实测掉 " +
                  std::to_string(99999 - ctx.getPet(1).hp - foe_loss) + "）");
        std::cout << "  [T1770] 1387 盾破反噬 通过\n";
    }

    // ══ T1771 1659 暴击转吸取（细雪迷离眼 [200,300,200,300]）══
    {
        // a) 必暴：吸取分支（对手掉 [200,306]；自身回 = 实际扣血 ≤300）
        {
            ScenarioBattle b(
                make_party(mk_self({kXiXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            set_crit_sixteenths(ctx.getPet(0), 16);
            ctx.getPet(0).hp = 4000;                  // 预置缺口（防回吸钳制）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int gain = ctx.getPet(0).hp - 4000;
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(ctx.crit_happened[0], "T1771.a 前置：必暴已打出");
            CHECK(foe_loss >= 200 && foe_loss <= 306,
                  "T1771.a 1659：吸取区间 [200,300]+红伤（实测掉 " +
                      std::to_string(foe_loss) + "）");
            CHECK(gain >= 200 && gain <= 300,
                  "T1771.a 1659：按实际扣血回吸（实测回 " + std::to_string(gain) + "）");
        }
        // b) 零暴对照：固伤分支（不回血）
        {
            ScenarioBattle b(
                make_party(mk_self({kXiXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int gain = ctx.getPet(0).hp - 4000;
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(!ctx.crit_happened[0], "T1771.b 前置：零暴");
            CHECK(foe_loss >= 200 && foe_loss <= 306,
                  "T1771.b 1659 对照：固伤区间 [200,300]+红伤（实测掉 " +
                      std::to_string(foe_loss) + "）");
            CHECK(gain == 0,
                  "T1771.b 1659 对照：固伤分支不回血（实测回 " + std::to_string(gain) + "）");
        }
        std::cout << "  [T1771] 1659 暴击转吸取 通过\n";
    }

    std::cout << "scenario_1770 PASS (J87 盾破反噬/暴击吸取族 1387/1659)" << std::endl;
    return 0;
}
