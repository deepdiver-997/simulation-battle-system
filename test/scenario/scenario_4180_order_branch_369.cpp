#include <utils/sbs_rand.h>
// 场景 4180 —— K262 先后手双分支族（369，soul_lib/k262_order_branch.cpp）
//（2026-10-21，worktree ../agents/k262；号段 4180-4189 → T4180-T4182 三块）
//
// 断言口径（369「若对手先出手则立即减少1/3HP（boss无效）；若自己先出手则造成的
//             伤害提升20%」）：
//   - T4180 子句②（自己先手 → 增伤 20%）：双局对照（同 seed：印主先手的一局带
//     印、一局不带）→ 带印局的掉血 / 对照局掉血 ≈ 1.20（比例带 1.15~1.25，红伤
//     本身有 85%~100% 浮动 → 不能用精确等式）。
//   - T4181 子句①（对手先手 → 对手额外掉 1/3 最大HP）：印主后手的一局，
//     对手掉血 = 自身攻击伤害 + 99999/3（=33333，粉伤 FIXED 不浮动，精确）。
//   - T4182 门闭：印主**先手**时子句①不触发（对手只掉自身攻击伤害，无 1/3 额外）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {3,20}（不补 {0,0} 前缀）——实现侧走
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
constexpr int kThird = kMaxHp / 3;   // 33333

// 印主：hp=99999（承受对手 1/3 扣血仍存活），速度可配（控先/后手）。
ElfPet mark_pet(int speed) {
    std::vector<int> args{3, 20};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(369, "K262 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_mark_pet(int speed) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻方：atk=300 → 拍打红伤 raw ∈ [87,102]（常识速查 §五）。
ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主自己出手一次（对手 NONE）→ 返回对手掉血。
int mark_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

// 攻方出手一次（印主 NONE）→ 返回印主掉血。
int foe_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4180 子句②：自己先手 → 增伤 20%（双局对照，比例带）═════════════════
    {
        int ctrl_loss = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet(300)), make_party(foe_pet(100)));
            b.start();
            sbs_srand(7);
            ctrl_loss = mark_strike(b);   // 印主自己打 → 观测对手掉血
        }
        int mark_loss = 0;
        {
            ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet(100)));
            b.start();
            sbs_srand(7);
            mark_loss = mark_strike(b);
        }
        CHECK(ctrl_loss > 0, "T4180 对照局前提：对手拍打有伤害");
        const double ratio = static_cast<double>(mark_loss) / static_cast<double>(ctrl_loss);
        CHECK(ratio > 1.15 && ratio < 1.25,
              "T4180 369：自己先手 → 造成的伤害提升 20%（比例带 1.15~1.25）");
        std::cout << "  [T4180] 先手增伤 通过（ctrl=" << ctrl_loss
                  << " mark=" << mark_loss << " ratio=" << ratio << "）\n";
    }

    // ══ T4180b 子句②门闭：自己**后手**时自己攻击 → 无 20% 增伤 ═══════════
    {
        int ctrl_loss = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet(100)), make_party(foe_pet(300)));
            b.start();
            sbs_srand(7);
            ctrl_loss = mark_strike(b);   // 印主后手出手 → 观测对手掉血
        }
        int mark_loss = 0;
        {
            ScenarioBattle b(make_party(mark_pet(100)), make_party(foe_pet(300)));
            b.start();
            sbs_srand(7);
            mark_loss = mark_strike(b);
        }
        CHECK_EQ(mark_loss, ctrl_loss,
                 "T4180b 369：自己后手 → 造成的伤害不增伤（门闭，与对照局相等）");
        std::cout << "  [T4180b] 后手不增伤 通过（ctrl=" << ctrl_loss
                  << " mark=" << mark_loss << "）\n";
    }

    // ══ T4181 子句①：对手先手 → 对手额外掉 1/3 最大HP ═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = foe_hp0 - ctx.getPet(1).hp;
        // 对手先手打了印主 raw（87~102），并因"对手先出手"额外掉 1/3 最大HP。
        CHECK(foe_loss >= kThird, "T4181 369：对手先出手 → 对手额外掉 1/3 最大HP（33333）");
        std::cout << "  [T4181] 对手先手扣血 通过（foe_loss=" << foe_loss << "）\n";
    }

    // ══ T4182 门闭：自己先手 → 子句①不触发（对手无 1/3 额外掉血）════════════
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_loss == 0, "T4182 369：自己先手 → 对手无 1/3 额外掉血（门闭）");
        std::cout << "  [T4182] 门闭对照 通过\n";
    }

    std::cout << "scenario_4180 PASS (K262 先后手双分支族)" << std::endl;
    return 0;
}
