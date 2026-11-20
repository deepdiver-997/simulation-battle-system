// 场景 5440 —— J217 未击败下回合减伤族（1548，moves_lib/j_next_round_reduce_if_no_kill.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5440-5449）
//
// 断言口径（ws 加算减伤槽精确值）：
//   - 1548（夜灵闪光 36485 纯链 [1548]|[50]）：当回合未击败对手 → R2 ROUND_START 写
//     ws.damage_reduce_add[0][首空槽] = 50（正值=减伤 50%）。
//   - a) 对手 99999 血存活 → R2 槽内出现 50（且仅一处新写）。
//   - b) 对照：R1 不施放 → R2 全槽 0。
//   - c) 负例：R1 击杀 1 血对手 → R2 不得出现 50（未击败门闭；死切驱动同 970 协议）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kYeLing  = 36485;  // [1548]|[50]（纯链；未击败→下回合减伤 50%）
constexpr int kMaxHp   = 5000;
constexpr int kPct     = 50;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kYeLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int count_pct_slots(const BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        if (ctx.ws.damage_reduce_add[side][i] == kPct) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5440 1548 未击败 → 下回合减伤 50% ══
    {
        {   // (a) 对手存活 → R2 槽内 50
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_pct_slots(ctx, 0), 1,
                     "T5440.a 1548：未击败→R2 减伤槽 +50（命中槽数 " +
                         std::to_string(count_pct_slots(ctx, 0)) + "，应 1）");
        }
        {   // (b) 对照：R1 不施放 → R2 全槽 0
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_pct_slots(ctx, 0), 0,
                     "T5440.b 1548 对照：未施放 → R2 无减伤槽");
        }
        {   // (c) 负例：R1 击杀 1 血对手 → R2 不得有 50
            ElfPet fodder = mk_foe(/*hp=*/1);
            ElfPet mate = mk_foe();
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(mk_self()), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T5440.c 前置：停在死切");
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            // 换人后还要再驱动一个完整回合：R2 的 ROUND_START 才会跑（减伤 tick 的时点）。
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_pct_slots(ctx, 0), 0,
                     "T5440.c 1548：当回合击败对手 → 门闭无减伤（命中槽数 " +
                         std::to_string(count_pct_slots(ctx, 0)) + "，应 0）");
        }
        std::cout << "  [T5440] 1548 未击败减伤 + 未施放对照 + 击杀门负例 通过\n";
    }

    std::cout << "scenario_5440 PASS (J217 未击败下回合减伤族)" << std::endl;
    return 0;
}
