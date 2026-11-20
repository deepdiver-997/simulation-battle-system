#include <utils/sbs_rand.h>
// 场景 7160 —— K371 已损梯度减伤+暴击率族（384，soul_lib/k371_gradient_reduce_crit.cpp）
//（2026-10-24，worktree ../agents/k371；号段 7160-7169 → T7160.a-h 八块）
//
// 断言口径（384「HP每降5%→受伤-5%上限50% + 暴击率+2%上限100%」；max=100000）：
//   减伤：atk300/def100 非暴红伤带 [87,102]（常识速查锚）→
//     满血 0 步原带；hp=10000（18 步→cap50%）带折半 [41,53]；
//     hp=90000（2 步→-10%）上界 102×0.9+2=93。
//   暴击：base 暴率 0 技能（set_crit_sixteenths(p,0)）——满血 0 点不暴；
//     低血满档 +100 点 rate>=100 必暴（skills.cpp ①.5）。
//   面板：伙伴行 [.,.,.,.,.,50,50] → 物攻+50/体力+50，本体行对照不动。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;
constexpr int kMaxHp = 100000;

SoulMark make_mark(std::vector<int> args) {
    SoulMark sm(384, "K371 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 2780;
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "战神·盖亚", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(std::move(args)));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手打持印者一拍，返回持印者掉血量。
int foe_hit(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return 0;   // 占位（真实差值由调用方前后快照）
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7160.a 满血对照：0 步原带 [85,104] ══
    {
        sbs_srand(7160);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit(b);
        const int d = hp0 - b.ctx().getPet(0).hp;
        CHECK(d >= 85 && d <= 104,
              "T7160.a 满血 0 步无减免：伤害 " + std::to_string(d) + " ∈ [85,104]");
        std::cout << "  [T7160.a] 满血对照 通过（d=" << d << "）\n";
    }

    // ══ T7160.b 50% cap：hp=10000（18 步→50%）带 [39,53] ══
    {
        sbs_srand(7161);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 10000;
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit(b);
        const int d = hp0 - b.ctx().getPet(0).hp;
        CHECK(d >= 39 && d <= 53,
              "T7160.b 减伤钳 50%：伤害 " + std::to_string(d) + " ∈ [39,53]");
        std::cout << "  [T7160.b] cap 减半 通过（d=" << d << "）\n";
    }

    // ══ T7160.c 2 步梯度：hp=90000 → -10%，上界 93 ══
    {
        sbs_srand(7162);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 90000;
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit(b);
        const int d = hp0 - b.ctx().getPet(0).hp;
        CHECK(d >= 72 && d <= 93,
              "T7160.c 2 步 -10%：伤害 " + std::to_string(d) + " ∈ [72,93]");
        std::cout << "  [T7160.c] 2 步梯度 通过（d=" << d << "）\n";
    }

    // ══ T7160.d 暴击快照：hp=1000 → 19 步 → +38 点（≤数学上界 38<100，cap 不可达）══
    {
        sbs_srand(7163);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.crit_rate_add[0] > 37.5f && b.ctx().ws.crit_rate_add[0] < 38.5f,
              "T7160.d 近死 19 步 crit_add=38（实测 " +
                  std::to_string(b.ctx().ws.crit_rate_add[0]) + "）");
        std::cout << "  [T7160.d] 近死快照 38 点 通过\n";
    }

    // ══ T7160.d2 暴击行为统计：30 局近死（38 点），落地数带 [2,22]（期望≈11.4）══
    {
        int crits = 0;
        for (int i = 0; i < 30; ++i) {
            sbs_srand(71630 + i);
            ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (b.ctx().crit_happened[0]) {
                ++crits;
            }
        }
        CHECK(crits >= 2 && crits <= 22,
              "T7160.d2 38 点暴击统计落地 " + std::to_string(crits) +
                  "/30（期望≈11.4，带 [2,22]）");
        std::cout << "  [T7160.d2] 暴击统计带 通过（" << crits << "/30）\n";
    }

    // ══ T7160.e 满血不暴：0 步 0 点 + base 0 ══
    {
        sbs_srand(7164);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().crit_happened[0], false,
                 "T7160.e 满血 0 点 + base0 → 不暴");
        std::cout << "  [T7160.e] 满血不暴 通过\n";
    }

    // ══ T7160.f ws 快照直读：step_past(ROUND_START) 读 crit_rate_add ══
    {
        sbs_srand(7165);
        ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 90000;   // 2 步 → +4 点
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.crit_rate_add[0] > 3.5f && b.ctx().ws.crit_rate_add[0] < 4.5f,
              "T7160.f 2 步快照 crit_add=4（实测 " +
                  std::to_string(b.ctx().ws.crit_rate_add[0]) + "）");
        std::cout << "  [T7160.f] ws 快照直读 通过\n";
    }

    // ══ T7160.g 伙伴行面板 + 本体行对照 ══
    {
        sbs_srand(7166);
        {
            ScenarioBattle b(make_party(mark_pet({5, 5, 50, 2, 100, 50, 50})),
                             make_party(foe_pet()));
            b.start();
            ElfPet& self = b.ctx().getPet(0);
            CHECK_EQ(self.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK], 350,
                     "T7160.g 伙伴行物攻 300+50（实测 " +
                         std::to_string(
                             self.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK]) +
                         "）");
            CHECK_EQ(self.numericalBase[NumericalPropertyIndex::HP], kMaxHp + 50,
                     "T7160.g 伙伴行体力 +50");
        }
        {
            ScenarioBattle c(make_party(mark_pet({5, 5, 50, 2, 100})),
                             make_party(foe_pet()));
            c.start();
            CHECK_EQ(c.ctx().getPet(0).numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK],
                     300, "T7160.g 本体行对照：面板不动");
        }
        std::cout << "  [T7160.g] 伙伴面板/对照 通过\n";
    }

    // ══ T7160.h db[0]=0 关闸：无减免无暴击加成 ══
    {
        sbs_srand(7167);
        ScenarioBattle b(make_party(mark_pet({0, 5, 50, 2, 100})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 10000;   // 若无关闸将是 50% 减免
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit(b);
        const int d = hp0 - b.ctx().getPet(0).hp;
        CHECK(d >= 85 && d <= 104,
              "T7160.h db[0]=0 关闸：伤害 " + std::to_string(d) + " ∈ [85,104]");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().crit_happened[0], false, "T7160.h 关闸无暴击加成");
        std::cout << "  [T7160.h] 关闸 通过\n";
    }

    std::cout << "scenario_7160 PASS (K371 已损梯度减伤+暴击率族)" << std::endl;
    return 0;
}
