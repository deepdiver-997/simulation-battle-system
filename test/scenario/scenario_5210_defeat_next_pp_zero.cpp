// 场景 5210 —— J198 击败→下只随机PP归零族（1663，moves_lib/j_defeat_next_pp_zero.cpp）
//（2026-10-22，号段 5210-5219）
//
// 断言口径：
//   - 1663（极霆威压 36932 纯链 [1663]|[2]，物 140，atk=3000）：
//     · (a) R1 击杀对手前锋（hp=50）→ 对手补位 slot1 登场 → **新登场宠**随机 2 个
//       技能 PP 归零（数 pp==0 的槽数 == 2，审计槽 990166301+1 == 2；不猜随机到哪两槽）。
//     · (b) 对照：前锋 hp=99999 未被击败 → R2 对手主动中切 slot1 → 新宠零个 PP 归零
//       （绝对量断言，不写"与对照相等"）。
//   - 只作用于"下只"：pending 在登场时清零；本场景 (a) 断言旧宠已死、新宠才被清零。
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

constexpr int kPaiDa  = 10004;  // 拍打：物 40
constexpr int kJiTing = 36932;  // [1663]|[2]（击败 → 下只随机 2 槽 PP 归零）
constexpr int kPendingAudit = 990166300;
constexpr int kLandedAudit = 990166301;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kJiTing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/3000, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_front(int hp) {
    ElfPet p = make_pet(2, "对手前锋", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

// 替补：5 个有限 PP 槽（每槽 10 点，便于"清零槽数"精确断言）。
ElfPet mk_bench() {
    ElfPet p = make_pet(2, "对手替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    for (Skills& s : p.skills) {
        s.maxPP = 10;
        s.pp = 10;
    }
    return p;
}

std::array<ElfPet, 6> foe_party(int front_hp) {
    ElfPet front = mk_front(front_hp);
    ElfPet bench = mk_bench();
    return {front, bench, bench, bench, bench, bench};
}

int storage_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

int zero_pp_slots(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp == 0) {
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

    // ══ T5210 1663 击败 → 下只随机 2 槽 PP 归零 ══
    {
        {   // (a) 击杀 50 血前锋 → 替补登场 → 恰好 2 槽归零
            ScenarioBattle b(make_party(mk_owner()), foe_party(/*front_hp=*/50));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH,
                     "T5210.a 前置：前锋被击败 → 停在对手选替补");
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 1, "T5210.a 前置：slot1 替补已登场");
            CHECK_EQ(storage_int(ctx, kLandedAudit + 1), 2,
                     "T5210.a 1663：新登场宠 2 槽 PP 归零（审计 " +
                         std::to_string(storage_int(ctx, kLandedAudit + 1)) + "）");
            CHECK_EQ(zero_pp_slots(ctx, 1), 2,
                     "T5210.a 1663：替补恰好 2 槽 pp==0（实测 " +
                         std::to_string(zero_pp_slots(ctx, 1)) + "）");
            CHECK_EQ(storage_int(ctx, kPendingAudit + 1), 0,
                     "T5210.a 1663：pending 已消费（只作用于下只）");
        }
        {   // (b) 对照：未击败 → R2 主动中切 → 新宠零个 PP 归零
            ScenarioBattle b(make_party(mk_owner()), foe_party(/*front_hp=*/99999));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(b.state(), State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
                     "T5210.b 前置：未击败 → 进入下一回合选择");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 1, "T5210.b 前置：slot1 主动中切登场");
            CHECK_EQ(zero_pp_slots(ctx, 1), 0,
                     "T5210.b 1663 对照：未击败 → 新宠无 PP 归零（实测 " +
                         std::to_string(zero_pp_slots(ctx, 1)) + "）");
            CHECK_EQ(storage_int(ctx, kLandedAudit + 1), -1,
                     "T5210.b 1663 对照：审计槽缺席");
        }
        std::cout << "  [T5210] 1663 击败 → 下只随机 2 槽 PP 归零 + 未击败对照 通过\n";
    }

    std::cout << "scenario_5210 PASS (J198 击败→下只随机PP归零族)" << std::endl;
    return 0;
}
