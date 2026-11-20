#include <utils/sbs_rand.h>
// 场景 6170 —— J284 被消强反伤窗族（1474，moves_lib/j_stat_removed_pink.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6170-6179 → T6170.a-f）
//
// 断言口径：
//   - 载体 28399 安适一隅（属性技，纯链 [1474]|[3,80]）→ 3 回合窗；自身每被消除
//     1 项能力提升 → 对**动手方**附加 80 点固定伤害。
//   - 用精确槽位快照（ability_levels[0][0]/[0][4]）而非"等级和"；固定伤害是绝对量
//     160/320，不是比例带（坑集 §2）。
//   - 事件"入队+晚 drain"：原语调用后手动 `event_center_.drain(...)` 泵一次，
//     与先例 scenario_124 F 段同口径。
#include <any>
#include <array>
#include <iostream>
#include <map>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 28399;   // 安适一隅：1474=[3,80]（纯链）
constexpr int kFiller  = 10004;   // 拍打
constexpr int kMaxHp   = 99999;
constexpr int kFixed   = 80;

constexpr int kTriggerAuditBase = 991474000;
constexpr int kDamageAuditBase  = 991474100;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self(bool with_window) {
    ElfPet p = make_pet(1, "己方",
                        {with_window ? kCarrier : kFiller, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet make_foe() {
    ElfPet p = make_pet(2, "对手", {kFiller, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// self_slot = -1 → 己方 NONE；否则用该槽技能。对手固定用 filler。
void one_round(ScenarioBattle& b, int self_slot) {
    if (self_slot >= 0) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    } else {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    }
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto& core = core_api();

    // ══ T6170.a 窗内：被对手消除 2 项提升 → 对手吃 2×80=160 固定伤害 ══
    {
        sbs_srand(20261022u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 50000;
        one_round(b, 0);   // r0 挂 3 回合窗（28399 属性技）
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 0, "T6170.a 挂窗后尚未触发");

        // 精确槽位快照：攻 +2、速 +1（不同槽，验证逐项计数）
        (void)core.stat_change(&x, 0, /*stat=*/0, /*delta=*/2);
        (void)core.stat_change(&x, 0, /*stat=*/4, /*delta=*/1);
        const int atk_before = x.ability_levels[0][0];
        const int spd_before = x.ability_levels[0][4];
        const int foe_before = x.getPet(1).hp;

        const int cleared = core.clear_stat_boosts_as(&x, 0, /*actor=*/1);
        x.event_center_.drain(&x, x.roundCount);

        CHECK_EQ(cleared, 2, "T6170.a 前置：对手消除 2 项提升");
        CHECK_EQ(atk_before, 2, "T6170.a 快照：消除前攻 +2");
        CHECK_EQ(spd_before, 1, "T6170.a 快照：消除前速 +1");
        CHECK_EQ(x.ability_levels[0][0], 0, "T6170.a 攻槽被清零");
        CHECK_EQ(x.ability_levels[0][4], 0, "T6170.a 速槽被清零");
        CHECK_EQ(foe_before - x.getPet(1).hp, kFixed * 2,
                 "T6170.a 逐项附加 2×80=160（实测差="
                     + std::to_string(foe_before - x.getPet(1).hp) + "）");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 1, "T6170.a 外窗触发 1 次");
        CHECK_EQ(audit_value(x, kDamageAuditBase + 0), kFixed * 2,
                 "T6170.a 审计累计附加 160");
        std::cout << "  [T6170.a] 窗内消 2 项 → 160 通过\n";
    }

    // ══ T6170.b 窗内但无提升可消 → 不触发、无伤害 ══
    {
        sbs_srand(6170u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 50000;
        one_round(b, 0);
        const int foe_before = x.getPet(1).hp;
        const int cleared = core.clear_stat_boosts_as(&x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(cleared, 0, "T6170.b 前置：无提升可消");
        CHECK_EQ(x.getPet(1).hp, foe_before, "T6170.b 无消除项 → 无附加伤害");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 0, "T6170.b 未触发");
        std::cout << "  [T6170.b] 无提升可消 通过\n";
    }

    // ══ T6170.c 窗外（3 回合后）→ 消除不触发 ══
    {
        sbs_srand(6171u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        one_round(b, 0);    // r0 挂窗
        one_round(b, -1);   // r1
        one_round(b, -1);   // r2；回合末 cleanup：roundCount - 0 >= 3 → 过期
        // 两项提升 = 两个槽（+2 单槽仍只算 1 项，勿按等级值计数）
        (void)core.stat_change(&x, 0, 0, 2);
        (void)core.stat_change(&x, 0, 4, 1);
        const int foe_before = x.getPet(1).hp;
        const int cleared = core.clear_stat_boosts_as(&x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(cleared, 2, "T6170.c 前置：窗外仍能消掉 2 项");
        CHECK_EQ(x.getPet(1).hp, foe_before, "T6170.c 窗外消除不附加伤害");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 0, "T6170.c 窗过期未触发");
        std::cout << "  [T6170.c] 窗外不触发 通过\n";
    }

    // ══ T6170.d 自己清自己（actor == self）→ 不算"被消除" ══
    {
        sbs_srand(6172u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 50000;
        one_round(b, 0);
        // 两个槽 = 2 项（+2 单槽只算 1 项）
        (void)core.stat_change(&x, 0, 0, 2);
        (void)core.stat_change(&x, 0, 4, 1);
        const int foe_before = x.getPet(1).hp;
        const int cleared = core.clear_stat_boosts_as(&x, 0, /*actor=*/0);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(cleared, 2, "T6170.d 前置：自身消除 2 项");
        CHECK_EQ(x.ability_levels[0][0], 0, "T6170.d 攻槽仍被清零");
        CHECK_EQ(x.getPet(1).hp, foe_before, "T6170.d 自己清自己 → 不附伤");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 0, "T6170.d 未触发");
        std::cout << "  [T6170.d] 自己清自己 通过\n";
    }

    // ══ T6170.e 未挂窗（对照）→ 消除不触发、审计槽未建立 ══
    {
        sbs_srand(6173u);
        ScenarioBattle b(make_party(make_self(false)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        // 两个槽 = 2 项（+2 单槽只算 1 项）
        (void)core.stat_change(&x, 0, 0, 2);
        (void)core.stat_change(&x, 0, 4, 1);
        const int foe_before = x.getPet(1).hp;
        const int cleared = core.clear_stat_boosts_as(&x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(cleared, 2, "T6170.e 前置：消除 2 项");
        CHECK_EQ(x.getPet(1).hp, foe_before, "T6170.e 无窗 → 不附伤");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), -2,
                 "T6170.e 无窗 → 审计槽未建立");
        std::cout << "  [T6170.e] 未挂窗对照 通过\n";
    }

    // ══ T6170.f 窗内两次独立消除 → 逐事件结算累计 320 ══
    {
        sbs_srand(6174u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 50000;
        one_round(b, 0);
        const int foe_before = x.getPet(1).hp;
        for (int i = 0; i < 2; ++i) {
            // 每次两个槽 = 2 项 → 每次附加 2×80=160
            (void)core.stat_change(&x, 0, 0, 2);
            (void)core.stat_change(&x, 0, 4, 1);
            (void)core.clear_stat_boosts_as(&x, 0, 1);
            x.event_center_.drain(&x, x.roundCount);
        }
        CHECK_EQ(foe_before - x.getPet(1).hp, kFixed * 4,
                 "T6170.f 两次消除各 2 项 → 累计 4×80=320（实测差="
                     + std::to_string(foe_before - x.getPet(1).hp) + "）");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 2, "T6170.f 触发 2 次");
        CHECK_EQ(audit_value(x, kDamageAuditBase + 0), kFixed * 4,
                 "T6170.f 审计累计 320");
        std::cout << "  [T6170.f] 两次独立消除 通过\n";
    }

    // ══ T6170.g 被消除的不是自己（对手清自己的提升）→ 不触发 ══
    // 目的：隔离"ev.target == self"门——对手自清时 actor=1 合法、只有 target 门能挡住。
    {
        sbs_srand(6175u);
        ScenarioBattle b(make_party(make_self(true)), make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 50000;
        one_round(b, 0);
        // 给**对手**两个槽提升，再让对手清自己（actor=1、target=1）
        (void)core.stat_change(&x, 1, 0, 2);
        (void)core.stat_change(&x, 1, 4, 1);
        const int foe_before = x.getPet(1).hp;
        const int cleared = core.clear_stat_boosts_as(&x, 1, /*actor=*/1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(cleared, 2, "T6170.g 前置：对手自清 2 项");
        CHECK_EQ(x.getPet(1).hp, foe_before, "T6170.g 非自身被消 → 不附伤");
        CHECK_EQ(audit_value(x, kTriggerAuditBase + 0), 0, "T6170.g 未触发");
        std::cout << "  [T6170.g] 非自身被消 通过\n";
    }

    std::cout << "scenario_6170 PASS (J284 被消强反伤窗族)" << std::endl;
    return 0;
}
