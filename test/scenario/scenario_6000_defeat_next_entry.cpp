// 场景 6000 —— J269 击败后下只登场族（601/2162，moves_lib/j_defeat_next_entry.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6000-6009 → T6000.a-d 四块）
//
// 断言口径（沿用 J198 1663 场景 5210 的击杀→补位流程）：
//   - 601 载体 毁灭屏障 18153 [601,697]|[0,0,0,0,2,0]：六槽**速 +2**（官方码序位 4
//     → 引擎槽 4，双序同值）。新登场替补 assert ability_levels[1][4]==2、其余槽 0；
//     审计 996012200+1 == 1（实际施加槽数），armed 标记归 0（只作用于下只）。
//   - 2162 载体 鲨鱼斩舰刀 38125 [1352,1289,2162]|[1,1,16]：{0}=16 流血；链兄弟 1352/1289
//     在自身无能力下降时惰性。新登场替补 assert 流血(16) 在身 + 审计 1。
//   - 对照块：前锋未死 → R1 对手主动中切 → 无 buff / 无异常（绝对值断言）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物 40
constexpr int kHuiMie = 18153;   // 毁灭屏障 [601,697]|[0,0,0,0,2,0]（速+2）
constexpr int kShaYu  = 38125;   // 鲨鱼斩舰刀 [1352,1289,2162]|[1,1,16]（流血 16）

constexpr int k601Audit = 996012200;    // +opp
constexpr int k601Armed = 996010200;    // +opp
constexpr int k2162Audit = 992162300;   // +opp

constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);   // 16

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet mk_owner(int first_skill) {
    ElfPet p = make_pet(1, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/3000, /*hp=*/5000);
    always_hit(p);
    return p;
}

ElfPet mk_front(int hp) {
    ElfPet p = make_pet(2, "对手前锋", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

ElfPet mk_bench() {
    ElfPet p = make_pet(2, "对手替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, /*hp=*/5000);
    always_hit(p);
    return p;
}

std::array<ElfPet, 6> foe_party(int front_hp) {
    ElfPet front = mk_front(front_hp);
    ElfPet bench = mk_bench();
    return {front, bench, bench, bench, bench, bench};
}

void kill_and_replace(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6000.a 601：本回合击败 → 下只登场速 +2 ══
    {
        ScenarioBattle b(make_party(mk_owner(kHuiMie)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6000.a 前置：slot1 替补已登场");
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T6000.a 601：新登场宠速度 +2（引擎槽4）");
        int others = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (i != 4 && ctx.ability_levels[1][i] != 0) {
                ++others;
            }
        }
        CHECK_EQ(others, 0, "T6000.a 601：其余槽为 0");
        CHECK_EQ(storage_int(ctx, k601Audit + 1, -1), 1, "T6000.a 601：审计施加 1 槽");
        CHECK_EQ(storage_int(ctx, k601Armed + 1, -1), 0, "T6000.a 601：pending 已消费");
        std::cout << "  [T6000.a] 601 击败→下只速+2 通过\n";
    }

    // ══ T6000.b 601 对照：未击败 → 主动中切不触发 ══
    {
        ScenarioBattle b(make_party(mk_owner(kHuiMie)), foe_party(/*front_hp=*/99999));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(b.state(), State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
                 "T6000.b 前置：未击败 → 下一回合选择");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "T6000.b 前置：slot1 主动中切登场");
        int nonzero = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (ctx.ability_levels[1][i] != 0) {
                ++nonzero;
            }
        }
        CHECK_EQ(nonzero, 0, "T6000.b 601 对照：未击败 → 新宠无能力变化（绝对值）");
        CHECK_EQ(storage_int(ctx, k601Audit + 1, -1), -1,
                 "T6000.b 601 对照：审计槽缺席");
        std::cout << "  [T6000.b] 601 未击败对照 通过\n";
    }

    // ══ T6000.c 2162：下次击败 → 下只登场流血 ══
    {
        ScenarioBattle b(make_party(mk_owner(kShaYu)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6000.c 前置：slot1 替补已登场");
        CHECK(ctx.has_active_abnormal_status(1, kBleed),
              "T6000.c 2162：新登场宠流血(16)在身");
        CHECK_EQ(storage_int(ctx, k2162Audit + 1, -1), 1, "T6000.c 2162：审计落地=1");
        std::cout << "  [T6000.c] 2162 击败→下只流血 通过\n";
    }

    // ══ T6000.d 2162 对照：未击败 → 主动中切不附加异常 ══
    {
        ScenarioBattle b(make_party(mk_owner(kShaYu)), foe_party(/*front_hp=*/99999));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(b.state(), State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
                 "T6000.d 前置：未击败 → 下一回合选择");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "T6000.d 前置：slot1 主动中切登场");
        int anomalies = 0;
        for (int id = 0; id <= 43; ++id) {
            if (ctx.has_active_abnormal_status(1, id)) {
                ++anomalies;
            }
        }
        CHECK_EQ(anomalies, 0, "T6000.d 2162 对照：新宠无异常（绝对值）");
        CHECK_EQ(storage_int(ctx, k2162Audit + 1, -1), -1,
                 "T6000.d 2162 对照：审计槽缺席");
        std::cout << "  [T6000.d] 2162 未击败对照 通过\n";
    }

    std::cout << "scenario_6000 PASS (J269 击败后下只登场族)" << std::endl;
    return 0;
}
