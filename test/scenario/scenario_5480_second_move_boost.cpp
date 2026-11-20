// 场景 5480 —— J219 后手增伤族（1718，moves_lib/j_second_move_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5480-5489）
//
// 断言口径（审计槽精确乘区 + 伤害绝对带 + 出手侧快照）：
//   - 1718（破灭天启剑 37484，物 160 priority=0，切片 [50,50]）：
//     · (a) 后手 + 对手体力更高：乘区 = 100+50+50 = 200（审计精确）；对手掉血 ×2。
//     · (b) 后手 + 对手体力更低：乘区 = 100+50     = 150（审计精确）；仅基础提升。
//     · (c) 先手：不武装（审计缺席 -1）；伤害为未提升基础带。
//   - 伤害基础带推导（速查 §五锚）：(0.84×300/100×160+2)=405.2 ×[0.85,1.0]
//     → 基础 [344,405]；×150% → [516,607]；×200% → [688,810]（各外扩 ±2 余量）。
//   - 后手前提由"对手先手拍打确实命中自身"（owner 掉血 [85,104]）侧证，避免用例
//     没走到后手分支却假绿。
//   - 载体选择：36671 炫色星辰 priority=3 天然先手，不能测后手门；取 priority=0 的
//     37484（链兄弟 1589 无异常空跑 / 1304 只挂下回合，不污染本刀伤害）。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa     = 10004;  // 拍打：物 40（填充/先手刀）
constexpr int kPoMie     = 37484;  // [1718,1589,1304]|[50,50,2,2,30]（1718 段 [50,50]）
constexpr int kAuditKey  = 990171800;  // + owner*2

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int speed, int hp) {
    ElfPet p = make_pet(1, "己方", {kPoMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int speed, int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5480 1718 后手增伤 ══
    {
        {   // (a) 后手 + 对手体力更高 → 乘区 200
            ScenarioBattle b(make_party(mk_owner(/*speed=*/100, /*hp=*/8000)),
                             make_party(mk_foe(/*speed=*/300, /*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int owner_before = ctx.getPet(0).hp;
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int foe_loss = foe_before - ctx.getPet(1).hp;
            const int owner_loss = owner_before - ctx.getPet(0).hp;
            CHECK(owner_loss >= 85 && owner_loss <= 104,
                  "T5480.a 前置：对手先手拍打命中 → 自身掉血 [85,104]（实测 " +
                      std::to_string(owner_loss) + "）");
            CHECK_EQ(audit(ctx), 200,
                     "T5480.a 1718：后手+对手体力更高 → 乘区 200（审计 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(foe_loss >= 686 && foe_loss <= 812,
                  "T5480.a 1718：伤害 ×200% → [688,810]（实测 " +
                      std::to_string(foe_loss) + "）");
        }
        {   // (b) 后手 + 对手体力更低 → 乘区 150
            ScenarioBattle b(make_party(mk_owner(/*speed=*/100, /*hp=*/99999)),
                             make_party(mk_foe(/*speed=*/300, /*hp=*/8000)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int owner_before = ctx.getPet(0).hp;
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int foe_loss = foe_before - ctx.getPet(1).hp;
            const int owner_loss = owner_before - ctx.getPet(0).hp;
            CHECK(owner_loss >= 85 && owner_loss <= 104,
                  "T5480.b 前置：对手先手拍打命中 → 自身掉血 [85,104]（实测 " +
                      std::to_string(owner_loss) + "）");
            CHECK_EQ(audit(ctx), 150,
                     "T5480.b 1718：后手+对手体力不高于自身 → 乘区 150（审计 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(foe_loss >= 514 && foe_loss <= 610,
                  "T5480.b 1718：伤害 ×150% → [516,607]（实测 " +
                      std::to_string(foe_loss) + "）");
        }
        {   // (c) 先手 → 不武装，基础伤害
            ScenarioBattle b(make_party(mk_owner(/*speed=*/300, /*hp=*/8000)),
                             make_party(mk_foe(/*speed=*/100, /*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int foe_loss = foe_before - ctx.getPet(1).hp;
            CHECK_EQ(audit(ctx), -1,
                     "T5480.c 1718 对照：先手 → 审计缺席（实测 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(foe_loss >= 342 && foe_loss <= 407,
                  "T5480.c 1718 对照：先手不提升 → 基础 [344,405]（实测 " +
                      std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T5480] 1718 后手增伤（对手高体额外）+ 低体仅基础 + 先手对照 通过\n";
    }

    std::cout << "scenario_5480 PASS (J219 后手增伤族)" << std::endl;
    return 0;
}
