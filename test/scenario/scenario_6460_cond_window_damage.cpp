// 场景 6460 —— J313 · 降态/异常回合末固伤窗族（439）+ 受击降攻族（189）
//（resources/moves_lib/j_cond_window_damage.cpp，2026-10-23，号段 6460-6469）
//
// 断言口径（逐条对应实现口径；**精确槽位** + **生效前后快照**，不写"等级和"）：
//   - 439（载体：森罗万象 21896 纯链 [439]|[5,200]，属性能技 power=0 → 自身零伤害；
//     对手掉血 = 固伤精确值，无 85%~100% 浮动带）：
//     (a) 己方降态 → R1 回合末对手**精确**掉 200、审计=1（门真开：己方防御槽 2 = -1）
//     (b) 对照无门 → 对手掉血 0、审计缺席
//     (c) 己方异常（睡眠）→ R2 回合末对手掉 200、审计=1
//     (d) 门只看**自身**：降态在对手侧 → 不触发（与 1731"双方任意一方"口径区分）
//     (e) 窗长 5：R1-R5 各触发一次（审计=5、对手累计精确掉 1000），R6 过期不再触发
//   - 189（载体：倔强 21618 纯链 [189]|[3]，属性能技；官方文案"3回合内若受到攻击，
//     对手的攻击和特攻将被降低"与 effect 189 模板自洽）：
//     (f) 己方受红伤 → 攻击方物攻槽 0 / 特攻槽 1 各 -1（逐槽快照比对，其余 4 槽不变）
//     (g) 对照：己方未受击 → 攻击方六槽全不变
//     (h) 窗长 3：R1/R2/R3 各受击一次 → 逐次 -1（累计 -3），R4 过期不再降
//     (i) 免弱对照：攻击方挂 STAT_DROP 免疫 → 弱化被挡下、六槽不变
#include <array>
#include <iostream>
#include <any>
#include <string>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSenLuoWanXiang = 21896;  // [439]|[5,200] 属性能技（纯链载体）
constexpr int kJueQiang       = 21618;  // [189]|[3]    属性能技（纯链载体）
constexpr int kPaiDa          = 10004;  // 拍打：物 40（side_effect=[] 纯技能）
constexpr int kAudit439       = 990439100;
constexpr int kSlotCount      = 6;  // 能力等级槽：0攻 1特攻 2防 3特防 4速 5命中
constexpr int kSlotAtk        = 0;
constexpr int kSlotSpAtk      = 1;
constexpr int kSlotDef        = 2;

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
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int audit439(const BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAudit439);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

std::array<int, kSlotCount> levels_of(const BattleContext& ctx, int side) {
    std::array<int, kSlotCount> out{};
    for (int i = 0; i < kSlotCount; ++i) {
        out[i] = ctx.ability_levels[side][i];
    }
    return out;
}

// 一回合：side0 是否用载体系（true=用 0 号技能），side1 是否用拍打。
void play_round(ScenarioBattle& b, bool owner_uses_carrier, bool foe_attacks) {
    b.submit_operation(0, owner_uses_carrier ? BattleFsm::ActionType::SELECT_SKILL
                                              : BattleFsm::ActionType::NONE,
                       0);
    b.submit_operation(1, foe_attacks ? BattleFsm::ActionType::SELECT_SKILL
                                      : BattleFsm::ActionType::NONE,
                       0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ 439 · {0}回合内若自身处于能力下降或异常状态，则对手每回合受到{1}点固定伤害 ══
    {   // (a) 己方降态 → R1 回合末对手精确掉 200
        ScenarioBattle b(make_party(mk_owner(kSenLuoWanXiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto before = levels_of(ctx, 0);
        (void)stat_drop(&ctx, /*target=*/0, /*stat=*/kSlotDef, /*amount=*/1);
        const auto dropped = levels_of(ctx, 0);
        CHECK_EQ(dropped[kSlotDef], before[kSlotDef] - 1,
                 "T6460.a 前置：己方降态（防御槽 2 = -1）");
        const int foe_before = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);
        const int loss = foe_before - ctx.getPet(1).hp;
        CHECK_EQ(loss, 200,
                 "T6460.a 439：己方降态 → R1 回合末对手精确掉 200（实测 " +
                     std::to_string(loss) + "）");
        CHECK_EQ(audit439(ctx), 1,
                 "T6460.a 439：审计=1（本窗触发 1 回合，实测 " +
                     std::to_string(audit439(ctx)) + "）");
    }
    {   // (b) 对照：无门 → 对手不掉血、审计缺席
        ScenarioBattle b(make_party(mk_owner(kSenLuoWanXiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_before = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);
        const int loss = foe_before - ctx.getPet(1).hp;
        CHECK_EQ(loss, 0,
                 "T6460.b 439 对照：无降态无异常 → 对手不掉血（实测 " +
                     std::to_string(loss) + "）");
        CHECK_EQ(audit439(ctx), -1, "T6460.b 439 对照：审计槽缺席");
    }
    {   // (c) 己方异常（睡眠）→ R2 回合末触发
        ScenarioBattle b(make_party(mk_owner(kSenLuoWanXiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);   // R1 无门
        CHECK_EQ(audit439(ctx), -1, "T6460.c 前置：R1 无门不触发");
        (void)apply_anomaly(&ctx, /*target=*/0,
                            static_cast<int>(AbnormalStatusId::Sleep),
                            /*duration_rounds=*/3, /*actor=*/1);
        const int foe_before = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/false);  // R2
        const int loss = foe_before - ctx.getPet(1).hp;
        CHECK_EQ(loss, 200,
                 "T6460.c 439：己方睡眠 → R2 回合末对手精确掉 200（实测 " +
                     std::to_string(loss) + "）");
        CHECK_EQ(audit439(ctx), 1, "T6460.c 439：审计=1");
    }
    {   // (d) 门只看**自身**：降态在对手侧 → 不触发（与 1731"双方任意一方"区分）
        ScenarioBattle b(make_party(mk_owner(kSenLuoWanXiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_drop(&ctx, /*target=*/1, /*stat=*/kSlotDef, /*amount=*/1);
        const int foe_before = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);
        const int loss = foe_before - ctx.getPet(1).hp;
        CHECK_EQ(loss, 0,
                 "T6460.d 439：降态在对手侧 → 不触发（门只看自身，实测掉血 " +
                     std::to_string(loss) + "）");
        CHECK_EQ(audit439(ctx), -1, "T6460.d 439：审计槽缺席");
    }
    {   // (e) 窗长 5：R1-R5 各触发一次，R6 过期
        ScenarioBattle b(make_party(mk_owner(kSenLuoWanXiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_drop(&ctx, /*target=*/0, /*stat=*/kSlotDef, /*amount=*/1);
        const int foe_before = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);   // R1
        CHECK_EQ(audit439(ctx), 1, "T6460.e 439：R1 触发一次");
        for (int r = 2; r <= 5; ++r) {
            play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/false);
        }
        CHECK_EQ(audit439(ctx), 5,
                 "T6460.e 439：R1-R5 共触发 5 次（窗长 5，实测 " +
                     std::to_string(audit439(ctx)) + "）");
        CHECK_EQ(foe_before - ctx.getPet(1).hp, 1000,
                 "T6460.e 439：对手累计精确掉 1000（5×200）");
        const int hp_r6 = ctx.getPet(1).hp;
        play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/false);  // R6 过期
        CHECK_EQ(ctx.getPet(1).hp, hp_r6, "T6460.e 439：R6 窗过期 → 对手不再掉血");
        CHECK_EQ(audit439(ctx), 5, "T6460.e 439：R6 审计仍=5");
    }

    // ══ 189 · {0}回合内若受到攻击，对手攻击等级-1、特攻等级-1 ══
    {   // (f) 己方受红伤 → 攻击方物攻/特攻各 -1，其余 4 槽不变
        ScenarioBattle b(make_party(mk_owner(kJueQiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto before = levels_of(ctx, 1);
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/true);
        const auto after = levels_of(ctx, 1);
        CHECK_EQ(after[kSlotAtk], before[kSlotAtk] - 1,
                 "T6460.f 189：攻击方物攻槽 0 = -1（实测 " +
                     std::to_string(after[kSlotAtk]) + "）");
        CHECK_EQ(after[kSlotSpAtk], before[kSlotSpAtk] - 1,
                 "T6460.f 189：攻击方特攻槽 1 = -1（实测 " +
                     std::to_string(after[kSlotSpAtk]) + "）");
        for (int i = 2; i < kSlotCount; ++i) {
            CHECK_EQ(after[i], before[i],
                     "T6460.f 189：攻击方槽 " + std::to_string(i) + " 不变（实测 " +
                         std::to_string(after[i]) + "）");
        }
    }
    {   // (g) 对照：己方未受击 → 攻击方六槽全不变
        ScenarioBattle b(make_party(mk_owner(kJueQiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto before = levels_of(ctx, 1);
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/false);
        const auto after = levels_of(ctx, 1);
        for (int i = 0; i < kSlotCount; ++i) {
            CHECK_EQ(after[i], before[i],
                     "T6460.g 189 对照：未受击 → 攻击方槽 " + std::to_string(i) +
                         " 不变（实测 " + std::to_string(after[i]) + "）");
        }
    }
    {   // (h) 窗长 3：R1/R2/R3 各受击一次 → 累计 -3；R4 过期不再降
        ScenarioBattle b(make_party(mk_owner(kJueQiang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/true);    // R1
        CHECK_EQ(levels_of(ctx, 1)[kSlotAtk], -1, "T6460.h 189：R1 受击 → 物攻 -1");
        play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/true);   // R2
        CHECK_EQ(levels_of(ctx, 1)[kSlotAtk], -2, "T6460.h 189：R2 受击 → 累计 -2");
        play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/true);   // R3
        CHECK_EQ(levels_of(ctx, 1)[kSlotAtk], -3, "T6460.h 189：R3 受击 → 累计 -3");
        play_round(b, /*owner_uses_carrier=*/false, /*foe_attacks=*/true);   // R4 窗过期
        CHECK_EQ(levels_of(ctx, 1)[kSlotAtk], -3, "T6460.h 189：R4 窗过期 → 物攻仍 -3");
        CHECK_EQ(levels_of(ctx, 1)[kSlotSpAtk], -3, "T6460.h 189：R4 窗过期 → 特攻仍 -3");
    }
    {   // (i) 免弱对照：攻击方挂 STAT_DROP 免疫 → 弱化被挡下、六槽不变
        ElfPet foe = mk_foe();
        foe.soulMark = scenario::make_stat_drop_immunity_soul_mark();
        ScenarioBattle b(make_party(mk_owner(kJueQiang)), make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*owner_uses_carrier=*/true, /*foe_attacks=*/true);
        const auto after = levels_of(ctx, 1);
        for (int i = 0; i < kSlotCount; ++i) {
            CHECK_EQ(after[i], 0,
                     "T6460.i 189 免弱对照：攻击方槽 " + std::to_string(i) +
                         " 仍为 0（弱化被 STAT_DROP 免疫挡下，实测 " +
                         std::to_string(after[i]) + "）");
        }
    }

    return 0;
}
