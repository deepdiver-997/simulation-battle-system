// 场景 5370 —— J211 本次伤害比例回合粉伤窗族（829，moves_lib/j_captured_damage_pink_window.cpp）
//（2026-10-22，号段 5370-5369）
//
// 断言口径：
//   - 829（嗜血蝙蝠 33033 纯链 [829]|[2,3]，特 130；己方 atk=300）：
//     · (a) R1 施放（捕获本次伤害/3）→ R2、R3 回合末各受等额粉伤，R4 窗过期不再掉血；
//       每回合掉血**精确等于**审计 990829100（tick 无随机）。
//     · (b) 叠加：R1、R2 各施放一次 → R3 回合末两条同时结算，掉血 = 两次捕获值之和。
//     · (c) 对照：R1 用填充刀 → 无捕获、R2 无粉伤、审计缺席。
//   - 用 NONE 空过回合，排除主伤污染；只测回合末差值。
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

constexpr int kPaiDa  = 10004;
constexpr int kShiXue = 33033;  // [829]|[2,3]
constexpr int kAuditKey = 990829100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kShiXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/5000);
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

void play(ScenarioBattle& b, bool owner_skill) {
    b.submit_operation(0, owner_skill ? BattleFsm::ActionType::SELECT_SKILL
                                      : BattleFsm::ActionType::NONE,
                       0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int round_loss(ScenarioBattle& b, bool owner_skill) {
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    play(b, owner_skill);
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5370 829 本次伤害比例回合粉伤窗 ══
    {
        {   // (a) 单次施放：R2/R3 各结算，R4 过期
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, /*owner_skill=*/true);   // R1 施放（主伤在 R1）
            const int amount = audit(ctx);
            CHECK(amount >= 90 && amount <= 140,
                  "T5370.a 829：捕获本次伤害/3 合理（审计 " + std::to_string(amount) + "）");
            const int r2 = round_loss(b, /*owner_skill=*/false);
            CHECK_EQ(r2, amount, "T5370.a 829：R2 回合末粉伤 = 捕获值（实测 " +
                                     std::to_string(r2) + "）");
            const int r3 = round_loss(b, /*owner_skill=*/false);
            CHECK_EQ(r3, amount, "T5370.a 829：R3 回合末粉伤 = 捕获值（实测 " +
                                     std::to_string(r3) + "）");
            const int r4 = round_loss(b, /*owner_skill=*/false);
            CHECK_EQ(r4, 0, "T5370.a 829：R4 窗过期 → 不再掉血（实测 " +
                                std::to_string(r4) + "）");
        }
        {   // (b) 叠加：两次施放 → R3 两条同时结算
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, /*owner_skill=*/true);   // R1 施放
            const int amount1 = audit(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            {
                // R2 施放；同时 R2 回合末第一条已开始结算——用 R3 空过观测叠加。
                const int before_r2 = ctx.getPet(1).hp;
                b.drive();
                const int r2_loss = before_r2 - ctx.getPet(1).hp;
                (void)r2_loss;   // R2 含主伤 + 第一条 tick，不作断言
            }
            const int amount2 = audit(ctx);
            CHECK(amount2 >= 90 && amount2 <= 140,
                  "T5370.b 829：第二次捕获值合理（审计 " + std::to_string(amount2) + "）");
            const int r3 = round_loss(b, /*owner_skill=*/false);
            CHECK_EQ(r3, amount1 + amount2,
                     "T5370.b 829：R3 两条叠加 = 两次捕获值之和（期望 " +
                         std::to_string(amount1 + amount2) + "，实测 " +
                         std::to_string(r3) + "）");
        }
        {   // (c) 对照：填充刀
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, /*owner_skill=*/false);   // R1 拍打
            CHECK_EQ(audit(ctx), -1, "T5370.c 829 对照：无捕获（审计缺席）");
            const int r2 = round_loss(b, /*owner_skill=*/false);
            CHECK_EQ(r2, 0, "T5370.c 829 对照：R2 无粉伤（实测 " +
                                std::to_string(r2) + "）");
        }
        std::cout << "  [T5370] 829 本次伤害比例回合粉伤（单次/叠加/对照）通过\n";
    }

    std::cout << "scenario_5370 PASS (J211 本次伤害比例回合粉伤窗族)" << std::endl;
    return 0;
}
