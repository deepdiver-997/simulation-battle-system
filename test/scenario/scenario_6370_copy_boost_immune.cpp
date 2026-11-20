// 场景 6370 —— J304 复制提升+异常次免族（1949，moves_lib/j_copy_boost_immune.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6370-6379）
//
// 断言口径：
//   - 1949（37318 秋叶静美 [1683,1949]|[3,1]，1949=[1]）：逐槽复制对手**正等级**，
//     对手等级**保持不动**（复制不是吸取）。
//   - 复制成功 → 自身挂 ANOMALY counts=1 次免票：第一次施加 TARGET_IMMUNE 并消耗，
//     第二次施加 SUCCESS 落地（直接用 apply_anomaly 原语驱动，确定性）。
//   - 对手无正等级 → 复制失败 → 不授票（第一次施加即落地）。
//   - 审计槽 991949100（复制正槽数）/991949200（次免次数）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1949 = 37318;   // 秋叶静美 [1683,1949]|[3,1] 物140
constexpr int kPaiDa = 10004;
constexpr int kBurn = 2;

constexpr int kCopiedKey = 991949100;   // +owner
constexpr int kImmuneKey = 991949200;   // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier1949, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void set_level(BattleContext& ctx, int side, int slot, int level) {
    ctx.ability_levels[side][slot] = level;
    ctx.ws.view_levels[side][slot] = level;
}

void run_carrier(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6370.a 复制 +2/+1：自身得到、对手不动、次免 1 次 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 1, 0, 2);   // 对手物攻 +2
        set_level(ctx, 1, 4, 1);   // 对手速度 +1
        run_carrier(b);
        CHECK_EQ(stor(ctx, kCopiedKey + 0), 2, "T6370.a 复制到 2 个正槽");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T6370.a 自身物攻 +2");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T6370.a 自身速度 +1");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T6370.a 对手物攻保持 +2（复制不是吸取）");
        CHECK_EQ(ctx.ability_levels[1][4], 1, "T6370.a 对手速度保持 +1");
        CHECK_EQ(stor(ctx, kImmuneKey + 0), 1, "T6370.a 授予异常次免 1 次");

        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "T6370.a 第一次异常被次免挡下");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T6370.a 第一次未挂烧伤");
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6370.a 次免用尽 → 第二次落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T6370.a 第二次挂上烧伤");
    }

    // ══ T6370.b 对手无正等级 → 复制失败 → 不授次免 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_carrier(b);
        CHECK_EQ(stor(ctx, kCopiedKey + 0), 0, "T6370.b 无可复制槽");
        CHECK_EQ(stor(ctx, kImmuneKey + 0), 0, "T6370.b 不授次免");
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6370.b 第一次异常直接落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T6370.b 挂上烧伤");
    }

    std::cout << "scenario 6370 OK" << std::endl;
    return 0;
}
