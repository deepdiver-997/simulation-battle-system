#include <utils/sbs_rand.h>
// 场景 2710 —— J169 反转封回血族（1 宏，moves_lib/j_reversal_healblock.cpp）
//（2026-10-04，worktree ../agents/j169；号段 2710-2719 → T2710 一块）
//
// 断言口径（对手治疗载体 = 能量滋养 28137 [466]|[200] cat4 pri0 纯链，基线已实现；
// 霜谕天寒叹 35198 [2] pri0——链 1522/1029 在 atk=1 低伤下不触发）：
//   a) 对手预置 +2 → R1 载体（反转成功：foe slot0=-2）+ 封回血 2 回合：
//      R2 对手能量滋养 → hp 不动（封住）；R4（窗过）→ +200 恢复。
//   b) 对照：对手无提升 → 反转失败 → R2 治疗正常 +200。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kShuangYu= 35198;  // 霜谕天寒叹 [1675,1522,1029]|[2]：特160（1675 载体）
constexpr int kNourZi  = 28137;  // 能量滋养 [466]|[200]：属性技（对手治疗载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2710.a/b 反转封回血（self 全程 NONE——干净隔离治疗走向）══
    // a) 对手 +2 → 反转成功（slot0=-2）→ R2 治疗被封（hp 不动）/ R3 恰好 +200（窗过）
    // b) 对照：对手无提升 → 反转失败 → R2 治疗即 +200
    for (int run = 0; run < 2; ++run) {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kShuangYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kNourZi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        if (run == 0) {
            (void)stat_change(&ctx, 1, 0, 2);   // 对手 +2（被反转对象）
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int hp1 = ctx.getPet(1).hp;
        if (run == 0) {
            CHECK_EQ(ctx.ability_levels[1][0], -2,
                     "T2710.a 前置：反转落地（foe slot0 实测 " +
                         std::to_string(ctx.ability_levels[1][0]) + "）");
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 self NONE
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手治疗
        b.drive();
        const int d2 = ctx.getPet(1).hp - hp1;
        if (run == 0) {
            CHECK_EQ(d2, 0,
                     "T2710.a 1675：封回血生效（R2 治疗无增量，实测 " +
                         std::to_string(d2) + "）");
            std::cout << "  [T2710.a] 反转封回血 通过\n";
        } else {
            CHECK_EQ(d2, 200,
                     "T2710.b 1675 对照：反转失败治疗 +200（实测 " +
                         std::to_string(d2) + "）");
            std::cout << "  [T2710.b] 反转失败对照 通过\n";
        }
    }

    std::cout << "scenario_2710 PASS (J169 反转封回血族 1675)" << std::endl;
    return 0;
}
