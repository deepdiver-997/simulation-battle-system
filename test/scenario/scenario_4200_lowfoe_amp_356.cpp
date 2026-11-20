#include <utils/sbs_rand.h>
// 场景 4200 —— K264 低体门增伤族（356，soul_lib/k264_lowfoe_amp.cpp）
//（2026-10-21，worktree ../agents/k264；号段 4200-4209 → T4200-T4201 两块）
//
// 断言口径（356「攻击时若对手当前体力小于自身最大HP的1/2，则对手受到的伤害提升50%」）：
//   - T4200 门开：印主满血（99999 → 阈值 49999）、对手低血（3000 < 49999）→
//     印主的攻击伤害提升 50%（双局对照：同 seed 一局带印、一局不带，比例带
//     1.40~1.60——红伤本身有 85%~100% 浮动，不能用精确等式）。
//   - T4201 门闭：对手满血（99999 ≥ 阈值 49999）→ 掉血与对照局相等（不增伤）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2,50}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：攻击载体
constexpr int kMaxHp = 99999;

// 印主：满血 99999（阈值 = max/2 = 49999）。
ElfPet mark_pet() {
    std::vector<int> args{2, 2, 50};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(356, "K264 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_mark_pet() {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主出手一次 → 返回对手掉血。对手 hp 可配（低血/满血两种门况）。
int mark_strike(ScenarioBattle& b, int foe_hp) {
    BattleContext& ctx = b.ctx();
    ctx.getPet(1).hp = foe_hp;
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4200 门开：对手低血（3000 < 49999）→ 增伤 50% ═════════════════════
    {
        int ctrl_loss = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet()), make_party(foe_pet()));
            b.start();
            sbs_srand(7);
            ctrl_loss = mark_strike(b, 3000);
        }
        int mark_loss = 0;
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
            b.start();
            sbs_srand(7);
            mark_loss = mark_strike(b, 3000);
        }
        CHECK(ctrl_loss > 0, "T4200 对照局前提：印主拍打有伤害");
        const double ratio = static_cast<double>(mark_loss) / static_cast<double>(ctrl_loss);
        CHECK(ratio > 1.40 && ratio < 1.60,
              "T4200 356：对手当前体力 < 自身最大HP的 1/2 → 伤害提升 50%（比例带 1.40~1.60）");
        std::cout << "  [T4200] 低体门增伤 通过（ctrl=" << ctrl_loss
                  << " mark=" << mark_loss << " ratio=" << ratio << "）\n";
    }

    // ══ T4201 门闭：对手满血（99999 ≥ 49999）→ 不增伤 ═══════════════════════
    {
        int ctrl_loss = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet()), make_party(foe_pet()));
            b.start();
            sbs_srand(7);
            ctrl_loss = mark_strike(b, 99999);
        }
        int mark_loss = 0;
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
            b.start();
            sbs_srand(7);
            mark_loss = mark_strike(b, 99999);
        }
        // 绝对断言（不与对照局比）：满血对手不该吃到 50% 增伤。
        // ⚠️ 若只比"与对照局相等"，两局同时增伤也会相等 → 门被破坏时该断言仍绿
        //   （2026-10-21 变异测试实测）→ 必须用绝对值断言。
        const double ratio = static_cast<double>(mark_loss) / static_cast<double>(ctrl_loss);
        CHECK(ratio < 1.10,
              "T4201 356：对手体力不低于 1/2 阈值 → 不增伤（比例 < 1.10）");
        std::cout << "  [T4201] 门闭对照 通过（ctrl=" << ctrl_loss
                  << " mark=" << mark_loss << "）\n";
    }

    std::cout << "scenario_4200 PASS (K264 低体门增伤族)" << std::endl;
    return 0;
}
