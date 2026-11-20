// 场景 6160 —— J283 对手属性技反应恢复/增益族（492，moves_lib/j_opp_attr_reaction.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6160-6169 → T6160.a-d）
//
// 断言口径：
//   - 载体 八尺勾玉决 17422（纯链，物理）→ 2 回合窗；对手使用**属性技能**时，
//     自身立刻恢复 max_hp/3 且 防御(槽2)+1、特防(槽3)+1（stat 码 1/3）。
//   - T6160.a 属性技触发：先压当前体力到 50000（避免满血 clamp，坑集 §4），
//     对手用缩头 20001（属性技，只改它自己的防御）→ 自身 hp=50000+33333=83333、
//     槽 2/3 各 +1，其余槽**逐槽快照相等**（不写"等级和"，坑集 §1）。
//   - T6160.b 攻击技不触发：对手改打拍打 10004 → 审计 0、hp 不变、各槽不变。
//   - T6160.c 窗过期：2 回合窗（rounds 0-1），第 3 回合对手用属性技不再触发。
#include <any>
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 17422;   // 八尺勾玉决：492=[2,3,1,1,3,1]
constexpr int kDengYan = 20004;   // 瞪眼（占位，不用）
constexpr int kSuoTou = 20001;    // 缩头：属性技，自身防御+1
constexpr int kPaiDa  = 10004;    // 拍打：攻击技
constexpr int kMaxHp  = 99999;
constexpr int kSetHp  = 50000;

constexpr int kTriggerAuditBase = 991492000;
constexpr int kHealAuditBase    = 991492100;
constexpr int kStatAuditBase    = 991492300;   // 与插件同段（2026-10-22 迁离 991492200）

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier, kSuoTou, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe() {
    ElfPet p = make_pet(2, "对手", {kSuoTou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// self_slot = -1 → 己方 NONE；foe_slot = -1 → 对手 NONE。
void one_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    if (self_slot >= 0) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    } else {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    }
    if (foe_slot >= 0) {
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    } else {
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    }
    b.drive();
}

std::array<int, BattleContext::kAbilityLevelSlotCount> slots(const BattleContext& ctx, int side) {
    std::array<int, BattleContext::kAbilityLevelSlotCount> out{};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        out[static_cast<std::size_t>(i)] = ctx.ability_levels[side][i];
    }
    return out;
}

// 逐槽比对：只有 allowed 列表里的槽允许变化，其余必须与 before 相同。
void check_only_slots_changed(const std::array<int, BattleContext::kAbilityLevelSlotCount>& before,
                              const std::array<int, BattleContext::kAbilityLevelSlotCount>& after,
                              int slot_a, int slot_b, const std::string& tag) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (i == slot_a || i == slot_b) {
            continue;
        }
        CHECK_EQ(after[static_cast<std::size_t>(i)], before[static_cast<std::size_t>(i)],
                 tag + " 槽" + std::to_string(i) + " 不变");
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6160.a 对手属性技 → 恢复 max/3 + 防御/特防各 +1 ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, -1);   // r0 挂 2 回合窗
        ctx.getPet(0).hp = kSetHp;
        const auto before = slots(ctx, 0);
        one_round(b, -1, 0);   // r1 对手缩头（属性技）
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 1,
                 "T6160.a 492：对手属性技触发 1 次");
        CHECK_EQ(audit_value(ctx, kHealAuditBase + 0), kMaxHp / 3,
                 "T6160.a 492：实际恢复 max_hp/3");
        CHECK_EQ(ctx.getPet(0).hp, kSetHp + kMaxHp / 3,
                 "T6160.a 492：当前体力精确 +33333");
        const auto after = slots(ctx, 0);
        CHECK_EQ(after[2], before[2] + 1, "T6160.a 492：防御槽(2) +1");
        CHECK_EQ(after[3], before[3] + 1, "T6160.a 492：特防槽(3) +1");
        check_only_slots_changed(before, after, 2, 3, "T6160.a");
        std::cout << "  [T6160.a] 属性技反应 通过 (hp=" << ctx.getPet(0).hp << ")\n";
    }

    // ══ T6160.b 对手攻击技 → 不触发 ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, -1);
        ctx.getPet(0).hp = kSetHp;
        const auto before = slots(ctx, 0);
        one_round(b, -1, 1);   // r1 对手拍打（攻击技）
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6160.b 492：攻击技不触发");
        CHECK(ctx.getPet(0).hp <= kSetHp, "T6160.b 492：未发生 max/3 恢复（仅可能掉血）");
        const auto after = slots(ctx, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(after[static_cast<std::size_t>(i)],
                     before[static_cast<std::size_t>(i)],
                     "T6160.b 492：槽" + std::to_string(i) + " 不变");
        }
        std::cout << "  [T6160.b] 攻击技不触发 通过\n";
    }

    // ══ T6160.c 窗过期：第 3 回合属性技不再触发 ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, -1);   // r0 挂窗（覆盖 r0/r1）
        one_round(b, -1, -1);  // r1
        ctx.getPet(0).hp = kSetHp;
        one_round(b, -1, 0);   // r2 对手缩头 → 窗已过期
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6160.c 492：窗外属性技不触发");
        CHECK_EQ(ctx.getPet(0).hp, kSetHp, "T6160.c 492：无恢复");
        std::cout << "  [T6160.c] 窗过期 通过\n";
    }

    // ══ T6160.d 只认"对手"：自身使用属性技不触发 ══
    {
        ScenarioBattle b(make_party(make_self()), make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, -1);   // r0 挂窗
        ctx.getPet(0).hp = kSetHp;
        one_round(b, 1, -1);   // r1 己方缩头（属性技），对手 NONE
        CHECK_EQ(audit_value(ctx, kTriggerAuditBase + 0), 0,
                 "T6160.d 492：自身属性技不触发（只认对手）");
        CHECK_EQ(ctx.getPet(0).hp, kSetHp, "T6160.d 492：无恢复");
        std::cout << "  [T6160.d] 只认对手属性技 通过\n";
    }

    std::cout << "scenario_6160 PASS (J283 对手属性技反应恢复/增益族)" << std::endl;
    return 0;
}
