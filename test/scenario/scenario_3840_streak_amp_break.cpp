// 场景 3840 —— K240 连击同型增伤/消除回合类族（776，soul_lib/k240_streak_amp_break.cpp）
//（2026-10-12，worktree ../agents/k240；号段 3840-3849 → T3840 一块六回合）
//
// 断言口径（行 [50,1]：连击攻击 +50% 伤、连击属性消回合）：
//   R1 攻（首用无增益，基线 d1）→ R2 攻（连击 +50%，d2）→ R3 属性（断连，失血 0）→
//   R4 攻（连击已断，d4≈d1）→ R5/R6 属性×2（连击属性走 break——裸对局无可消除物，
//   smoke 不炸即可；break 通道本体已由 K399 同款原语实证）。
//   · crit 全程置零（set_crit_sixteenths(0)），浮带只剩 85%~100%：d2∈[1.275,1.5]×base
//     严格 > d4∈[0.85,1.0]×base；d4 与 d1 同分布（宽松带）；d2 上界 1.8×d1。
//   · 对手全程 NONE：其 hp 差 = 印主红伤（唯一变量）。
//   ⚠️ 纯 drive() 流（3720 记档）。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;      // 撞击（攻击载体）
constexpr int kSuoTou = 20001;  // 缩头（属性技载体，效果落地与否不影响连击判定）
constexpr int kMaxHp = 99999;

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kSuoTou},
                        /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(776, "K240 魂印", "",
                                               EffectArgs(std::vector<int>{50, 1})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);
    return p;
}

// 印主出一技（skill_idx），对手 NONE；返回对手本回合失血。
int mark_uses(ScenarioBattle& b, int skill_idx) {
    const int before = b.pet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return before - b.pet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3840 连击账本六回合 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();

        const int d1 = mark_uses(b, 0);   // R1 攻：首用，无增益
        CHECK(d1 > 0, "T3840 776：R1 攻击造成伤害（基线）");
        const int d2 = mark_uses(b, 0);   // R2 攻：连击 +50%
        CHECK(d2 >= d1 * 5 / 4 && d2 <= d1 * 9 / 5,
              "T3840 776：连击攻击 +50%（d1=" + std::to_string(d1) +
                  " d2=" + std::to_string(d2) + "）");
        const int d3 = mark_uses(b, 4);   // R3 属性技：断连（属性技无伤）
        CHECK_EQ(d3, 0, "T3840 776：R3 属性技不失血（断连回合）");
        const int d4 = mark_uses(b, 0);   // R4 攻：连击已断
        CHECK(d4 >= d1 * 7 / 10 && d4 <= d1 * 13 / 10,
              "T3840 776：断连后无增益（d4=" + std::to_string(d4) + " vs d1=" +
                  std::to_string(d1) + "）");
        CHECK(d2 > d4, "T3840 776：连击伤严格高于断连伤（浮带不翻转）");
        const int d5 = mark_uses(b, 4);   // R5 属性：连击属性首用
        CHECK_EQ(d5, 0, "T3840 776：R5 属性技不失血");
        const int d6 = mark_uses(b, 4);   // R6 属性：连击属性 → break（裸局 no-op 不炸）
        CHECK_EQ(d6, 0, "T3840 776：R6 属性连击不失血（break 通道 smoke）");
        std::cout << "  [T3840] 连击账本六回合 通过\n";
    }

    std::cout << "scenario_3840 PASS (K240 连击同型增伤/消除回合类族)" << std::endl;
    return 0;
}
