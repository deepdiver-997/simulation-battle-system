#include <utils/sbs_rand.h>
// 场景 7130 —— K368 每回合恢复+攻击概率威力翻倍族（372，soul_lib/k368_round_heal_rollpower.cpp）
//（2026-10-24，worktree ../agents/k368；号段 7130-7139 → T7130-T7135 六块）
//
// 断言口径：
//   - 回血：max=1000000、db[0]=166 → 每回合 +166000（166‰=1/6）。
//   - 威力：技能 10001 基础威力视图 35；翻倍 → 70（K220 3620 同款停泊 FIRST_SKILL_EFFECT
//     读视图）。确定性支线用 args{1000,*} 千分满档短路；官方行 166‰ 走统计带。
//   - 伙伴行面板：args 尾段 [50,50] → 物攻面板 +50、体力面板 +50（特攻不动）。
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

constexpr int kPaiDa = 10001;   // 攻击技（威力视图基线 35，K220 探针同款）
constexpr int kMaxHp = 1000000;

SoulMark mark372(std::vector<int> args) {
    SoulMark sm(372, "K368 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 2780;   // 战神·盖亚（真实对局 row457 伙伴行绑定）
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "战神·盖亚", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, mark372(std::move(args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// K220 探针同款：停泊 FIRST_SKILL_EFFECT 读威力视图。
int probe(std::vector<int> args, int seed) {
    ScenarioBattle b(make_party(mark_pet(std::move(args))), make_party(foe_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(seed);
    b.start();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    b.drive();
    return view;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7130 常态回血 166‰ ═══════════════════════════════════════════════════
    {
        sbs_srand(7130);
        ScenarioBattle b(make_party(mark_pet({166, 166})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 500000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 666000,
                 "T7130 372：每回合恢复 max×166‰=166000（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7130] 常态回血 通过\n";
    }

    // ══ T7131 钳顶 ═══════════════════════════════════════════════════════════
    {
        sbs_srand(7131);
        ScenarioBattle b(make_party(mark_pet({166, 166})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 999000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp,
                 "T7131 372：恢复钳 max（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7131] 钳顶 通过\n";
    }

    // ══ T7132 db[0]=0 关闸（显式 0 不 fallback）═══════════════════════════════
    {
        sbs_srand(7132);
        ScenarioBattle b(make_party(mark_pet({0, 166})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 500000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 500000,
                 "T7132 372：db[0]=0 → 不恢复（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7132] 回血关闸 通过\n";
    }

    // ══ T7133 威力翻倍确定性（千分满档短路 / db[1]=0 关闸）════════════════════
    {
        CHECK_EQ(probe({1000, 1000}, 7133), 70,
                 "T7133 372：db[1]=1000 → 恒翻倍（35→70）");
        CHECK_EQ(probe({1000, 0}, 7134), 35,
                 "T7133 372：db[1]=0 → 恒不翻（35）");
        std::cout << "  [T7133] 威力门/关闸 通过\n";
    }

    // ══ T7134 官方行 166‰ 统计带（期望 ≈19.9/120，带 [6,40] ±3.4σ 以上）══════
    {
        int landed = 0;
        for (int i = 0; i < 120; ++i) {
            if (probe({166, 166}, 7140 + i) == 70) {
                ++landed;
            }
        }
        CHECK(landed >= 6 && landed <= 40,
              "T7134 372：166‰ 统计带落地 " + std::to_string(landed) +
                  "/120（期望≈20，带 [6,40]）");
        std::cout << "  [T7134] 官方行统计带 通过（" << landed << "/120）\n";
    }

    // ══ T7135 伙伴行面板（db[2]/db[3]：攻+50 体+50，特攻不动）═════════════════
    {
        sbs_srand(7135);
        ScenarioBattle b(make_party(mark_pet({166, 166, 50, 50})),
                         make_party(foe_pet()));
        b.start();
        ElfPet& self = b.ctx().getPet(0);
        CHECK_EQ(self.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK], 51,
                 "T7135 372：伙伴行物攻面板 1+50（实测 " +
                     std::to_string(
                         self.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK]) +
                     "）");
        CHECK_EQ(self.numericalBase[NumericalPropertyIndex::SPECIAL_ATTACK], 1,
                 "T7135 372：特攻面板不动（实测 " +
                     std::to_string(
                         self.numericalBase[NumericalPropertyIndex::SPECIAL_ATTACK]) +
                     "）");
        CHECK_EQ(self.numericalBase[NumericalPropertyIndex::HP], kMaxHp + 50,
                 "T7135 372：伙伴行体力面板 +50（实测 " +
                     std::to_string(self.numericalBase[NumericalPropertyIndex::HP]) +
                     "）");
        // 对照：本体行（无尾段）无面板
        sbs_srand(7136);
        ScenarioBattle c(make_party(mark_pet({166, 166})), make_party(foe_pet()));
        c.start();
        ElfPet& plain = c.ctx().getPet(0);
        CHECK_EQ(plain.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK], 1,
                 "T7135 372：本体行无面板对照（实测 " +
                     std::to_string(
                         plain.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK]) +
                     "）");
        std::cout << "  [T7135] 伙伴行面板/对照 通过\n";
    }

    std::cout << "scenario_7130 PASS (K368 每回合恢复+攻击概率威力翻倍族)" << std::endl;
    return 0;
}
