// 场景 5250 —— J200 提升态攻击回血族（489，moves_lib/j_boost_attack_heal.cpp）
//（2026-10-22，号段 5250-5259）
//
// 断言口径：
//   - 489（星粒碰撞 33921 纯链 [489]|[4]，物 140）：
//     · (a) 自身攻击 +1（提升态）且压到半血 2500 → 本刀落地后回 max/4=1250 →
//       hp 精确 3750；审计槽 990489100+0 == 1。
//     · (b) 对照：无任何正等级 → 不触发、hp 保持 2500（绝对值断言）。
//     · (c) 门取"任一正槽"：只给命中槽 +1（槽 5）→ 仍触发（六槽含命中）。
//   - 半血观测避免满血 clamp（坑集 §4/§9）；对手 NONE，排除对手伤害污染。
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

constexpr int kPaiDa    = 10004;  // 拍打：物 40
constexpr int kXingLi   = 33921;  // [489]|[4]（物 140）
constexpr int kMaxHp    = 5000;
constexpr int kHalfHp   = 2500;
constexpr int kAuditKey = 990489100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kXingLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5250 489 提升态 → 每次攻击回 max/{0} ══
    {
        {   // (a) 攻击 +1、半血 → 回 1250
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kHalfHp;   // 只压当前体力（坑集 §4）
            (void)stat_change(&ctx, /*target=*/0, /*stat=*/0, /*delta=*/1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kHalfHp + kMaxHp / 4,
                     "T5250.a 489：提升态攻击回 max/4（实测 hp " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            CHECK_EQ(audit(ctx), 1, "T5250.a 489：本刀触发 1 次（审计 " +
                                        std::to_string(audit(ctx)) + "）");
        }
        {   // (b) 无提升态 → 不回血
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kHalfHp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kHalfHp,
                     "T5250.b 489 对照：无提升态 → 不回血（实测 hp " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            CHECK_EQ(audit(ctx), -1, "T5250.b 489 对照：审计缺席");
        }
        {   // (c) 仅命中槽 +1 → 仍算提升态（六槽含命中）
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kHalfHp;
            (void)stat_change(&ctx, /*target=*/0, /*stat=*/5, /*delta=*/1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kHalfHp + kMaxHp / 4,
                     "T5250.c 489：命中槽 +1 亦为提升态 → 回血（实测 hp " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T5250] 489 提升态攻击回血 + 无提升对照 + 命中槽门 通过\n";
    }

    std::cout << "scenario_5250 PASS (J200 提升态攻击回血族)" << std::endl;
    return 0;
}
