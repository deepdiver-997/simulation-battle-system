// 场景 2060 —— J112 伤害不足吸取/低体回血反噬族（2 宏，moves_lib/j_insufficient_drain_lowheal.cpp）
//（2026-10-03，worktree ../agents/j112；号段 2060-2069 → T2060-T2061 两块）
//
// 断言口径：
//   - 947 = 血色心眼 19726 [947]|[300,4]（物140 纯链）：红伤带 [202,237] 恒 < 300
//     → 必吸取 1/4 = 24999：自身（hp=max/2 预设）回 24999 精确；对手掉 = 红伤带
//     + 24999 = [25201,25236]；阈值判定为"恒真"设计（阈值 300 高于带顶）；
//   - 997 = 烟火无情 31714 [889,997]|[22,2,4]（特85 prio3；889 消对手回合类空场
//     =良性，J102 同款验证）：997=[2,4]——hp=max/5（19999 < 49999 ✓）→ 回 1/4
//     = 24999 + 等量粉 24999；负例 = hp=max/2（49999 不< 49999）→ 无回无粉；
//   - 997 红伤带：特85·atk200 = 0.84×2×85+2 = 144.8 → [123,144]。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kXueSe  = 19726;   // 血色心眼[947]|[300,4]：物140（伤害不足→吸1/4）
constexpr int kYanHuo = 31714;   // 烟火无情[889,997]|[22,2,4]：特85（低体回1/4+等量粉）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2060 947 伤害不足 → 吸对手 1/4（血色心眼 [300,4]）══
    {
        // 红伤带 [202,237] 恒 < 300 → 必吸。自身 hp=max/2 承接回吸。
        ScenarioBattle b(make_party(self_pet({kXueSe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMaxHp / 2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int self_gain = ctx.getPet(0).hp - kMaxHp / 2;
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(self_gain, kMaxHp / 4,
                 "T2060 947：吸取回自身 1/4（实测回 " + std::to_string(self_gain) + "）");
        CHECK(foe_loss >= kMaxHp / 4 + 202 && foe_loss <= kMaxHp / 4 + 237,
              "T2060 947：对手掉红伤带+吸取（实测 " + std::to_string(foe_loss) +
                  "，带 [25201,25236]）");
        std::cout << "  [T2060] 947 伤害不足吸取 通过\n";
    }

    // ══ T2061 997 低体回 1/4 + 等量粉（烟火无情 [2,4]）══
    {
        // a) 正例：hp=max/5（<1/2）→ 回 24999 + 粉 24999（对手掉红伤带+24999）
        {
            ScenarioBattle b(make_party(self_pet({kYanHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 5;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int self_gain = ctx.getPet(0).hp - kMaxHp / 5;
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK_EQ(self_gain, kMaxHp / 4,
                     "T2061.a 997：低体回 1/4（实测回 " + std::to_string(self_gain) + "）");
            CHECK(foe_loss >= kMaxHp / 4 + 123 && foe_loss <= kMaxHp / 4 + 144,
                  "T2061.a 997：对手掉红伤带+等量粉（实测 " + std::to_string(foe_loss) +
                      "，带 [25122,25143]）");
        }
        // b) 负例：hp=max/2（不 < 1/2）→ 无回无粉
        {
            ScenarioBattle b(make_party(self_pet({kYanHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int self_gain = ctx.getPet(0).hp - kMaxHp / 2;
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK_EQ(self_gain, 0, "T2061.b 997 门：不低体无回血");
            CHECK(foe_loss >= 123 && foe_loss <= 144,
                  "T2061.b 997 门：无等量粉（仅红伤带，实测 " + std::to_string(foe_loss) +
                      "）");
        }
        std::cout << "  [T2061] 997 低体回血反噬 通过\n";
    }

    std::cout << "scenario_2060 PASS (J112 伤害不足吸取/低体回血反噬族 947/997)" << std::endl;
    return 0;
}
