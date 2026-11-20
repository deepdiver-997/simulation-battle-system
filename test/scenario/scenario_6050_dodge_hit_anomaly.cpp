// 场景 6050 —— J272 闪避窗命中异常族（1975，moves_lib/j_dodge_hit_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6050-6059 → T6050.a-c 三块）
//
// 断言口径：
//   - 闪避段：载体 烟霞震啸 28661 [191,1975,886]|[4,3,100,9,6,45]（1975=[3,100,9,6]）
//     → 100% 闪避，对手拍打必 MISS：自身 hp 绝对值不变、审计 -1、对手无 9/6 异常。
//   - 命中段：载体 走风步尘 28636 [191,1975,1320]|[4,2,80,9,28,2,3]（1975=[2,80,9,28]）
//     → 对手拍打置 must_hit（必中绕过闪避率链，SOP §三.4）必命中：自身掉血、
//     审计 ∈ {9,28} 且实际挂上同一个异常（精确码位，非"等级和"）。
//   - 攻击技过滤：同载体，对手改打属性技 瞪眼 20004（必中）→ EVENT_HIT 照样发，
//     但插件必须按 executed_skill_is_attack 过滤：自身防御 -1（证明属性技真的命中）、
//     审计保持 -1、对手无 9/28 异常。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYanXia  = 28661;   // 烟霞震啸：1975=[3,100,9,6]
constexpr int kZouFeng = 28636;   // 走风步尘：1975=[2,80,9,28]
constexpr int kPaiDa   = 10004;   // 拍打：攻击 filler
constexpr int kDengYan = 20004;   // 瞪眼：属性技（对手防御 -1）
constexpr int kMaxHp   = 99999;

constexpr int kAuditBase = 991975000;   // +owner：本次抽中异常码（-1=未触发）

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_attacker(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe(int first_skill, bool must_hit) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

bool anomaly_active(BattleContext& ctx, int side, int code) {
    return core_api().has_active_anomaly(&ctx, side, code);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6050.a 100% 闪避：对手攻击必 MISS（hp 绝对值不变）══
    {
        ScenarioBattle b(make_party(make_attacker(kYanXia)), make_party(make_foe(kPaiDa, false)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "T6050.a 1975：100% 闪避 → 自身 hp 不变");
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), -1, "T6050.a 1975：未命中 → 审计 -1");
        CHECK(!anomaly_active(ctx, 1, 9) && !anomaly_active(ctx, 1, 6),
              "T6050.a 1975：对手未被附加 9/6");
        std::cout << "  [T6050.a] 1975 100% 闪避 通过\n";
    }

    // ══ T6050.b 必中攻击穿透闪避 → 命中段随机附加 9/28 之一（精确码位）══
    {
        ScenarioBattle b(make_party(make_attacker(kZouFeng)), make_party(make_foe(kPaiDa, true)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        one_round(b);
        CHECK(ctx.getPet(0).hp < hp_before, "T6050.b 1975：必中攻击 → 自身掉血");
        const int code = audit_value(ctx, kAuditBase + 0);
        CHECK(code == 9 || code == 28,
              "T6050.b 1975：审计为 {2}/{3} 之一（实测 " + std::to_string(code) + "）");
        CHECK(code == 9 || code == 28 ? anomaly_active(ctx, 1, code) : false,
              "T6050.b 1975：对手实际挂上与审计一致的异常");
        std::cout << "  [T6050.b] 1975 命中段随机异常 通过 (code=" << code << ")\n";
    }

    // ══ T6050.c 属性技命中不算"攻击技能"：EVENT_HIT 过滤 ══
    {
        ScenarioBattle b(make_party(make_attacker(kZouFeng)),
                         make_party(make_foe(kDengYan, true)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);
        bool self_dropped = false;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            self_dropped = self_dropped || ctx.ability_levels[0][i] < 0;
        }
        CHECK(self_dropped, "T6050.c 前置：瞪眼属性技命中 → 自身某项能力下降");
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), -1,
                 "T6050.c 1975：属性技命中 → 审计保持 -1");
        CHECK(!anomaly_active(ctx, 1, 9) && !anomaly_active(ctx, 1, 28),
              "T6050.c 1975：属性技命中不附加异常");
        std::cout << "  [T6050.c] 1975 攻击技过滤 通过\n";
    }

    std::cout << "scenario_6050 PASS (J272 闪避窗命中异常族)" << std::endl;
    return 0;
}
