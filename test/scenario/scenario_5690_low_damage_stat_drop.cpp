// 场景 5690 —— J238 低伤降级族（501，moves_lib/j_low_damage_stat_drop.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5690-5699）
//
// 断言口径（精确槽位 + 效果前后快照 + 审计槽）：
//   - 501「若造成的伤害不足{0}，则对手{1}等级-{2}」：DETECT 攻方桶读本刀 final，
//     final < {0} → stat_drop(对手, 官方码 {1} 映射的引擎槽, {2})。
//   - 载体 14530 月牙无双刃 纯链 [501]|[300,1,1]（物 150）：
//     · (a) atk=1 → final≈3 <300 → 对手**防御槽 2** -1（精确槽位，其余槽 0）。
//     · (b) atk=400 → final≥300 → 对手全槽 0（绝对值门闭，不写"与对照相等"）。
//   - 槽位映射：37442 艳阳步 [240,3,2] → 官方码 3=特防 → 引擎槽 3 降 2（防槽 2 不动）；
//     16957 秘灵炎破斩 [501,501]|[300,1,1,300,3,1] → 双实例同时触发：
//     防槽 -1 且特防槽 -1（证明逐实例各自结算、且官方码 1/3 分别落 2/3）。
//   - 对照组：同 ATK 下改用无 501 的拍打 → 对手全槽 0（效果确由 501 驱动）。
//   - 审计槽 990501100+owner = 本刀实际伤害，保证"门的前提真的成立"。
//   - 效果前后快照：出手前记录对手 6 槽，出手后逐槽比对（坑集 §1 指定槽纪律）。
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

constexpr int kPaiDa     = 10004;  // 拍打：物 40（填充刀/对照）
constexpr int kYueYa     = 14530;  // [501]|[300,1,1]
constexpr int kYanYang   = 37442;  // [501]|[240,3,2]
constexpr int kMiLing    = 16957;  // [501,501]|[300,1,1,300,3,1]
constexpr int kAuditKey  = 990501100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
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

void use_carrier(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手 6 槽快照（本体等级；0攻1特攻2防3特防4速5命中）。
std::array<int, 6> opp_snapshot(BattleContext& ctx) {
    std::array<int, 6> s{};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        s[static_cast<std::size_t>(i)] = ctx.ability_levels[1][i];
    }
    return s;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5690 501 低伤 → 对手指定槽下降 ══
    {
        {   // (a) 低伤 → 对手防御槽 -1（官方码 1 → 引擎槽 2）
            ScenarioBattle b(make_party(mk_owner(kYueYa, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const auto before = opp_snapshot(ctx);
            use_carrier(b);
            const auto after = opp_snapshot(ctx);
            CHECK(audit(ctx) >= 0 && audit(ctx) < 300,
                  "T5690.a 501：本刀伤害 <300（审计 " +
                      std::to_string(audit(ctx)) + "）");
            CHECK_EQ(before[2], 0, "T5690.a 前快照：对手防槽为 0");
            CHECK_EQ(after[2], -1,
                     "T5690.a 501：对手**防御槽** -1（实测 " +
                         std::to_string(after[2]) + "）");
            CHECK_EQ(ctx.ws.view_levels[1][2], -1,
                     "T5690.a 视图同步：对手防槽视图 -1（伤害公式读取源）");
            for (int i : {0, 1, 3, 4, 5}) {
                CHECK_EQ(after[static_cast<std::size_t>(i)], 0,
                         "T5690.a 非目标槽 " + std::to_string(i) + " 不变");
            }
        }
        {   // (b) 高伤 → 门闭，不降级（绝对值断言）
            ScenarioBattle b(make_party(mk_owner(kYueYa, /*atk=*/400)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK(audit(ctx) >= 300,
                  "T5690.b 501：本刀伤害 ≥300（审计 " +
                      std::to_string(audit(ctx)) + "）");
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_levels[1][i], 0,
                         "T5690.b 高伤 → 对手槽 " + std::to_string(i) + " 不降");
            }
        }
        {   // (c) 官方码 3=特防 → 引擎槽 3，降 2；防槽 2 不动
            ScenarioBattle b(make_party(mk_owner(kYanYang, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK(audit(ctx) >= 0 && audit(ctx) < 240,
                  "T5690.c 501：本刀伤害 <240（审计 " +
                      std::to_string(audit(ctx)) + "）");
            CHECK_EQ(ctx.ability_levels[1][3], -2,
                     "T5690.c 37442 [240,3,2] → 对手特防槽 -2（实测 " +
                         std::to_string(ctx.ability_levels[1][3]) + "）");
            CHECK_EQ(ctx.ability_levels[1][2], 0,
                     "T5690.c 防槽不被误降（官方码→引擎槽映射正确）");
        }
        {   // (d) 双实例 [501,501]：防槽 -1 且特防槽 -1
            ScenarioBattle b(make_party(mk_owner(kMiLing, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK(audit(ctx) >= 0 && audit(ctx) < 300,
                  "T5690.d 501：本刀伤害 <300（审计 " +
                      std::to_string(audit(ctx)) + "）");
            CHECK_EQ(ctx.ability_levels[1][2], -1,
                     "T5690.d 实例一 [300,1,1] → 对手防槽 -1");
            CHECK_EQ(ctx.ability_levels[1][3], -1,
                     "T5690.d 实例二 [300,3,1] → 对手特防槽 -1");
        }
        {   // (e) 对照：同一出手方用无 501 的拍打 → 对手全槽 0
            ScenarioBattle b(make_party(mk_owner(kPaiDa, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_levels[1][i], 0,
                         "T5690.e 对照（无 501）对手槽 " + std::to_string(i) + " 不变");
            }
        }
        std::cout << "  [T5690] 501 低伤降级 + 高伤门闭 + 官方码映射 + 双实例 + 对照 通过\n";
    }

    std::cout << "scenario_5690 PASS (J238 低伤降级族)" << std::endl;
    return 0;
}
