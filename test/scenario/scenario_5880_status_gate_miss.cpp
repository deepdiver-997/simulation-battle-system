// 场景 5880 —— J257 异常门必 MISS 窗族（686，moves_lib/j_status_gate_miss.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5880-5889）
//
// 断言口径（HP 精确前后快照 + 审计槽 + 槽位精确）：
//   - miss = 被保护方（己方）HP 与回合前**逐值相等**；命中 = HP 严格下降（量不究，
//     85%~100% 浮动）。
//   - "下{1}回合"从下一回合起：施放当回合（R1）对手攻击**照常命中**（反证不做当回合
//     直写）；R2/R3 为 {1}=2 的两回合窗；R4 过期恢复命中。
//   - 门闭（对手无对应异常）→ 审计 0 且 R2/R3 照常命中；错异常码（要求烧伤却给中毒）
//     → 门闭。
//   - 审计槽 996860100+owner：1=门过 / 0=门闭。
//   - 载体参数驱动：16790 [中毒,2]；17268 [烧伤,2]。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物攻 40（填充 / 对手物理载体）
constexpr int k686a  = 16790;   // 幽藤绞杀破 [686,476]|[1,2] → 中毒门，2 回合
constexpr int k686b  = 17268;   // 极火爆冲杀 [686]|[2,2] → 烧伤门，2 回合

constexpr int kAudit686 = 996860100;

constexpr int kPoison = 1;
constexpr int kBurn   = 2;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/100, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);   // 对手保留非必中：must_hit 会绕过命中判定
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5880.a 16790 [中毒,2]：门过 → 下 2 回合必 MISS，当回合照中 ══
    {
        ScenarioBattle b(make_party(mk_owner(k686a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/10, /*actor=*/0);

        const int hp0 = ctx.getPet(0).hp;                    // 效果生效前快照
        both_act(b, 0, 0);                                   // R1 施放（对手拍打）
        const int hp1 = ctx.getPet(0).hp;
        CHECK(hp1 < hp0, "a1 施放当回合不武装：R1 对手照常命中（hp 下降）");
        CHECK_EQ(audit(ctx, kAudit686 + 0), 1, "a2 门过审计=1");

        const int hp2_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R2 窗内
        const int hp2 = ctx.getPet(0).hp;
        CHECK_EQ(hp2, hp2_before, "a3 窗内 R2 攻击必 MISS（hp 精确不变）");

        const int hp3_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R3 窗内（第 2 回合）
        const int hp3 = ctx.getPet(0).hp;
        CHECK_EQ(hp3, hp3_before, "a4 窗内 R3 攻击必 MISS（hp 精确不变）");

        const int hp4_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R4 窗过期
        const int hp4 = ctx.getPet(0).hp;
        CHECK(hp4 < hp4_before, "a5 窗过期 R4 恢复命中（hp 下降）");
        std::cout << "  [a] 16790 中毒门：R1 中 / R2-R3 miss / R4 中，审计=1\n";
    }

    // ══ T5880.b 16790 无异常：门闭，R2/R3 照常命中 ══
    {
        ScenarioBattle b(make_party(mk_owner(k686a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        both_act(b, 0, 0);                                   // R1 施放（无异常）
        CHECK_EQ(audit(ctx, kAudit686 + 0), 0, "b1 门闭审计=0");

        const int hp2_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R2
        CHECK(ctx.getPet(0).hp < hp2_before, "b2 门闭 R2 照常命中");

        const int hp3_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R3
        CHECK(ctx.getPet(0).hp < hp3_before, "b3 门闭 R3 照常命中");
        std::cout << "  [b] 无异常门闭：R2/R3 照常命中，审计=0\n";
    }

    // ══ T5880.c 17268 [烧伤,2]：参数驱动（正确异常门过 / 错异常门闭）══
    {
        ScenarioBattle b(make_party(mk_owner(k686b)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/10, /*actor=*/0);
        both_act(b, 0, 0);                                   // R1 施放（对手烧伤）
        CHECK_EQ(audit(ctx, kAudit686 + 0), 1, "c1 烧伤门过审计=1");
        const int hp2_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R2 窗内
        CHECK_EQ(ctx.getPet(0).hp, hp2_before, "c2 烧伤门 R2 必 MISS");
        std::cout << "  [c] 17268 烧伤门：R2 miss，审计=1\n";
    }

    // ══ T5880.d 17268 错异常（要求烧伤，实给中毒）：门闭 ══
    {
        ScenarioBattle b(make_party(mk_owner(k686b)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/10, /*actor=*/0);
        both_act(b, 0, 0);                                   // R1 施放（对手中毒≠烧伤）
        CHECK_EQ(audit(ctx, kAudit686 + 0), 0, "d1 错异常门闭审计=0");
        const int hp2_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                                   // R2
        CHECK(ctx.getPet(0).hp < hp2_before, "d2 错异常门闭 R2 照常命中");
        std::cout << "  [d] 错异常码：门闭，R2 照常命中\n";
    }

    return 0;
}
