// 场景 1550 —— J68 PP 门吸取族（2 宏，moves_lib/j_pp_gate_drain.cpp）
//（2026-10-03，worktree ../agents/j68；号段 1550-1559 → T1550-T1551 两块）
//
// 断言口径：
//   - PP 门造确定性 = 直改对手技能槽 pp（skills[1].pp=1 < 门槛 2 → 门开；对照不改
//     全满 → 门关）；fixed_damage 定点吸取无 85%-100% 浮动 → 回吸量精确：
//     1450 门开=2×150=300 / 门关=150；1632 门开=250+100=350 / 门关=250；
//   - 我方血量精确 +N（heal 按 dealt 回、无浮动）；对手掉血 = 吸取 + 我方攻击
//     红伤 ~2（窄带）；对手全程 NONE（NONE 侧被跳过无碍——不需要对手行动）；
//   - 载体短链兄弟（945/984）未实现=哑链，无噪声。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40
constexpr int kLingYe  = 33505;  // 灵叶箭矢 [945,1450]|[150,2]：特85（门开双吸 300）
constexpr int kShengYi = 34677;  // 圣意屠魔诀 [1632,984]|[250,2,100]：特150（额外 100）
constexpr int kMaxHp   = 5000;

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

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1550 1450 吸取+PP 门翻倍（灵叶箭矢 [150,2]）══
    {
        // a) 门开：对手槽 pp=1 <2 → 双吸 300（自 +300 精确）
        {
            ScenarioBattle b(
                make_party(mk_self({kLingYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;                  // 预置缺口（防回吸钳制）
            ctx.getPet(1).skills[1].pp = 1;           // 任一槽 PP<2 → 门开
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4300,
                     "T1550.a 1450：PP 门开 → 双吸回 300（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss >= 301 && foe_loss <= 304,
                  "T1550.a 前置：对手掉血=双吸300+攻击~2（实测 " +
                      std::to_string(foe_loss) + "）");
        }
        // b) 门关：对手 PP 全满 → 单吸 150
        {
            ScenarioBattle b(
                make_party(mk_self({kLingYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4150,
                     "T1550.b 1450 对照：PP 门关 → 单吸 150（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1550] 1450 PP门吸取 通过\n";
    }

    // ══ T1551 1632 吸取+PP 门额外（圣意屠魔诀 [250,2,100]）══
    {
        // a) 门开：250+100=350
        {
            ScenarioBattle b(
                make_party(mk_self({kShengYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;                  // 预置缺口（防回吸钳制）
            ctx.getPet(1).skills[1].pp = 1;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4350,
                     "T1551.a 1632：PP 门开 → 吸 250+额外 100（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 门关：单吸 250
        {
            ScenarioBattle b(
                make_party(mk_self({kShengYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4250,
                     "T1551.b 1632 对照：PP 门关 → 单吸 250（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1551] 1632 PP门额外吸取 通过\n";
    }

    std::cout << "scenario_1550 PASS (J68 PP 门吸取族 1450/1632)" << std::endl;
    return 0;
}
