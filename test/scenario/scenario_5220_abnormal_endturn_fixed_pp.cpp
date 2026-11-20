// 场景 5220 —— J199 异常/降态回合末固伤+削PP窗族（1731，moves_lib/j_abnormal_endturn_fixed_pp.cpp）
//（2026-10-22，号段 5220-5229）
//
// 断言口径：
//   - 1731（拈红渲紫 36845 纯链 [1731]|[4,300,2]，物 140，己方 atk=1 先手）：
//     · (a) 对手防御 -1（能力下降）→ R1 回合末触发：固定伤害 300 + 对手全技能 PP-2。
//       己方本刀主伤约 1-4，故对手掉血带 [300,310]；PP 逐槽 10→8；审计=1。
//     · (b) 双方无异常/无降态 → 不触发：掉血仅主伤 [1,4]，PP 全 10，审计缺席。
//     · (c) 己方自身睡眠（异常门"任意一方"）→ 下一回合末触发：固定 300、PP 10→8。
//     · (d) 窗长 4：R1-R4 每回合末各触发一次（PP 10→2，审计=4）；R5 过期不再触发
//       （R5 掉血 0、PP 仍 2——绝对值断言）。
//   - 固定伤害吃护盾（本场景无盾）；观测前对手 hp=99999 不会 clamp。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa     = 10004;  // 拍打：物 40
constexpr int kNianHong  = 36845;  // [1731]|[4,300,2]（物 140）
constexpr int kAuditKey  = 990173100;
constexpr int kMaxPP     = 10;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kNianHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    for (Skills& s : p.skills) {
        s.maxPP = kMaxPP;
        s.pp = kMaxPP;   // 固定起点，便于"每回合 -2"精确断言
    }
    return p;
}

int audit(const BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

bool all_pp_equal(BattleContext& ctx, int side, int expected) {
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp != expected) {
            return false;
        }
    }
    return true;
}

void play_round(ScenarioBattle& b, bool owner_uses_carrier) {
    b.submit_operation(0, owner_uses_carrier ? BattleFsm::ActionType::SELECT_SKILL
                                             : BattleFsm::ActionType::NONE,
                       owner_uses_carrier ? 0 : 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5220 1731 回合末条件固伤 + 全技能 PP-2 ══
    {
        {   // (a) 对手防御 -1 → R1 回合末触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, /*target=*/1, /*stat=*/2, /*amount=*/1);
            const int before = ctx.getPet(1).hp;
            play_round(b, /*owner_uses_carrier=*/true);
            const int loss = before - ctx.getPet(1).hp;
            CHECK(loss >= 300 && loss <= 310,
                  "T5220.a 1731：降态门触发 → 固定伤害 300（实测掉血 " +
                      std::to_string(loss) + "）");
            CHECK(all_pp_equal(ctx, 1, kMaxPP - 2),
                  "T5220.a 1731：对手所有技能 PP-2（10→8）");
            CHECK_EQ(audit(ctx), 1, "T5220.a 1731：本窗触发 1 回合（审计 " +
                                        std::to_string(audit(ctx)) + "）");
        }
        {   // (b) 双方无异常/无降态 → 不触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            play_round(b, /*owner_uses_carrier=*/true);
            const int loss = before - ctx.getPet(1).hp;
            CHECK(loss >= 1 && loss <= 4,
                  "T5220.b 1731 对照：无门 → 仅主伤 [1,4]（实测 " +
                      std::to_string(loss) + "）");
            CHECK(all_pp_equal(ctx, 1, kMaxPP), "T5220.b 1731 对照：PP 不变（仍 10）");
            CHECK_EQ(audit(ctx), -1, "T5220.b 1731 对照：审计槽缺席");
        }
        {   // (c) 己方自身睡眠 → 异常门"任意一方" → 下一回合末触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play_round(b, /*owner_uses_carrier=*/true);   // R1 无门，不触发
            CHECK_EQ(audit(ctx), -1, "T5220.c 前置：R1 无门不触发");
            (void)apply_anomaly(&ctx, /*target=*/0,
                                static_cast<int>(AbnormalStatusId::Sleep),
                                /*duration_rounds=*/3, /*actor=*/1);
            const int before = ctx.getPet(1).hp;
            play_round(b, /*owner_uses_carrier=*/false);   // R2 回合末
            const int loss = before - ctx.getPet(1).hp;
            CHECK_EQ(loss, 300, "T5220.c 1731：己方睡眠 → 对手固定 300（实测 " +
                                    std::to_string(loss) + "）");
            CHECK(all_pp_equal(ctx, 1, kMaxPP - 2),
                  "T5220.c 1731：异常门触发 → 对手 PP-2");
            CHECK_EQ(audit(ctx), 1, "T5220.c 1731：触发 1 回合");
        }
        {   // (d) 窗长 4：R1-R4 各触发，R5 过期不触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, /*target=*/1, /*stat=*/2, /*amount=*/1);
            play_round(b, /*owner_uses_carrier=*/true);   // R1
            play_round(b, /*owner_uses_carrier=*/false);  // R2
            play_round(b, /*owner_uses_carrier=*/false);  // R3
            play_round(b, /*owner_uses_carrier=*/false);  // R4
            CHECK_EQ(audit(ctx), 4, "T5220.d 1731：R1-R4 共 4 次触发（审计 " +
                                        std::to_string(audit(ctx)) + "）");
            CHECK(all_pp_equal(ctx, 1, kMaxPP - 8),
                  "T5220.d 1731：4 回合各 -2 → PP=2");
            const int hp_after_r4 = ctx.getPet(1).hp;
            play_round(b, /*owner_uses_carrier=*/false);  // R5（已过期）
            CHECK_EQ(ctx.getPet(1).hp, hp_after_r4,
                     "T5220.d 1731：R5 窗过期 → 不再固伤");
            CHECK(all_pp_equal(ctx, 1, kMaxPP - 8),
                  "T5220.d 1731：R5 窗过期 → PP 保持 2");
            CHECK_EQ(audit(ctx), 4, "T5220.d 1731：过期后审计不增");
        }
        std::cout << "  [T5220] 1731 异常/降态回合末固伤+削PP窗 通过\n";
    }

    std::cout << "scenario_5220 PASS (J199 异常/降态回合末固伤+削PP窗族)" << std::endl;
    return 0;
}
