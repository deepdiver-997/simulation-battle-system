// 场景 5840 —— J253 条件威力强化族（638/758，moves_lib/j_cond_power_amp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5840-5849）
//
// 断言口径（字段断言为主 + 伤害比值侧证 + 审计槽）：
//   - 两条宏在 SKILL_EFFECT 写 ws.skill_power_view（102 变威力通道）。按坑集 §6，
//     威力改写族**不能**用紧比例带当主断言：停泊到 BATTLE_FIRST_SKILL_EFFECT 后读
//     `ctx.ws.skill_power_view[0].value`，与同载体的无门基线做**字段精确等式**。
//   - 638（纯链载体）：16735 迅影幻光爪 base150 [中毒,50] → 225；37574 燃点爆发 base80
//     [烧伤,200] → 240；32950 掠影之角 base130 [失明,50] → 195；错误异常（给中毒但要求
//     烧伤）→ 80（字段严格等于基线）。
//   - 758（37844 漆黑扑击 base150 [混乱]）：对手/自身/双方任一侧有混乱 → 300（双方同时
//     有仍 300，证明是"×2"不是"每侧叠加"）；无混乱 → 150（绝对值）。
//   - 审计槽：996380100 = 门过 pct / -1 门闭；997580100 = 位掩码 1/2/3 / 0 门闭。
//   - 伤害比值侧证（全驱动，均值抹平 85%~100% 浮动）：638-37574 有/无烧伤 ≈3×；
//     758-37844 有/无混乱 ≈2×。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充）
constexpr int k638a  = 16735;   // 迅影幻光爪 [638]|[1,50] base150
constexpr int k638b  = 37574;   // 燃点爆发 [638]|[2,200] base80
constexpr int k638c  = 32950;   // 掠影之角 [638]|[20,50] base130
constexpr int k758   = 37844;   // 漆黑扑击 [758,1019]|[10,...] base150

constexpr int k638Audit = 996380100;
constexpr int k758Audit = 997580100;

constexpr int kPoison = 1;
constexpr int kBurn   = 2;
constexpr int kBlind  = 20;
constexpr int kConfusion = 10;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
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

struct FieldTrial {
    int view = -999;
    int audit = -999;
};

// apply_to: -1=不施加 / 0=自身 / 1=对手 / 2=双方
FieldTrial field_trial(int carrier, int anomaly, int apply_to) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (apply_to == 0 || apply_to == 2) {
        (void)apply_anomaly(&ctx, 0, anomaly, /*duration_rounds=*/5, /*actor=*/0);
    }
    if (apply_to == 1 || apply_to == 2) {
        (void)apply_anomaly(&ctx, 1, anomaly, /*duration_rounds=*/5, /*actor=*/0);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    FieldTrial t;
    t.view = ctx.ws.skill_power_view[0].value;
    t.audit = audit(ctx, (carrier == k638a || carrier == k638b || carrier == k638c
                              ? k638Audit
                              : k758Audit));
    return t;
}

int damage_trial(int carrier, int anomaly, int apply_to) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (apply_to == 0 || apply_to == 2) {
        (void)apply_anomaly(&ctx, 0, anomaly, /*duration_rounds=*/5, /*actor=*/0);
    }
    if (apply_to == 1 || apply_to == 2) {
        (void)apply_anomaly(&ctx, 1, anomaly, /*duration_rounds=*/5, /*actor=*/0);
    }
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5840.a 638：对手异常门 + 字段精确威力 ──
    {
        const FieldTrial a = field_trial(k638a, kPoison, /*apply_to=*/1);
        const FieldTrial b = field_trial(k638b, kBurn, /*apply_to=*/1);
        const FieldTrial c = field_trial(k638c, kBlind, /*apply_to=*/1);
        const FieldTrial miss = field_trial(k638b, kPoison, /*apply_to=*/1);   // 要求烧伤却给中毒
        const FieldTrial none = field_trial(k638b, kBurn, /*apply_to=*/-1);
        std::cout << "  [T5840.a] 638 views=" << a.view << "/" << b.view << "/" << c.view
                  << " wrong=" << miss.view << " none=" << none.view
                  << " audits=" << a.audit << "/" << b.audit << "/" << c.audit
                  << "/" << miss.audit << "/" << none.audit << std::endl;
        CHECK_EQ(a.view, 225, "T5840.a 16735 base150 +50% = 225");
        CHECK_EQ(b.view, 240, "T5840.a 37574 base80 +200% = 240");
        CHECK_EQ(c.view, 195, "T5840.a 32950 base130 +50% = 195");
        CHECK_EQ(miss.view, 80, "T5840.a 异常不匹配 → 威力保持基线 80（绝对）");
        CHECK_EQ(none.view, 80, "T5840.a 无异常 → 威力保持基线 80（绝对）");
        CHECK_EQ(a.audit, 50, "T5840.a 门过审计=50");
        CHECK_EQ(b.audit, 200, "T5840.a 门过审计=200");
        CHECK_EQ(c.audit, 50, "T5840.a 门过审计=50");
        CHECK_EQ(miss.audit, -1, "T5840.a 异常不匹配 → 审计 -1");
        CHECK_EQ(none.audit, -1, "T5840.a 无异常 → 审计 -1");
    }

    // ══ T5840.b 758：任一方的混乱门 + 字段精确翻倍（双侧不叠乘） ══
    {
        const FieldTrial foe = field_trial(k758, kConfusion, /*apply_to=*/1);
        const FieldTrial self = field_trial(k758, kConfusion, /*apply_to=*/0);
        const FieldTrial both = field_trial(k758, kConfusion, /*apply_to=*/2);
        const FieldTrial none = field_trial(k758, kConfusion, /*apply_to=*/-1);
        // 要求混乱却给中毒（⚠️ 不能用烧伤：自身烧伤会在物化时把物理威力减半，污染字段基线）
        const FieldTrial wrong = field_trial(k758, kPoison, /*apply_to=*/2);
        std::cout << "  [T5840.b] 758 views foe=" << foe.view << " self=" << self.view
                  << " both=" << both.view << " none=" << none.view
                  << " wrong=" << wrong.view
                  << " masks=" << foe.audit << "/" << self.audit << "/" << both.audit
                  << "/" << none.audit << "/" << wrong.audit << std::endl;
        CHECK_EQ(foe.view, 300, "T5840.b 对手混乱 → 150×2 = 300");
        CHECK_EQ(self.view, 300, "T5840.b 自身混乱 → 150×2 = 300");
        CHECK_EQ(both.view, 300, "T5840.b 双方混乱 → 仍 300（×2 不是每侧叠加）");
        CHECK_EQ(none.view, 150, "T5840.b 无混乱 → 威力保持基线 150（绝对）");
        CHECK_EQ(wrong.view, 150, "T5840.b 异常不匹配 → 威力保持基线 150（绝对）");
        CHECK_EQ(foe.audit, 2, "T5840.b 位掩码=对手(2)");
        CHECK_EQ(self.audit, 1, "T5840.b 位掩码=自身(1)");
        CHECK_EQ(both.audit, 3, "T5840.b 位掩码=双方(3)");
        CHECK_EQ(none.audit, 0, "T5840.b 门闭掩码=0");
        CHECK_EQ(wrong.audit, 0, "T5840.b 异常不匹配掩码=0");
    }

    // ══ T5840.c 伤害比值侧证（字段改写确实被伤害公式消费） ══
    {
        // ⚠️ 比值用失明(=不扣血异常)；烧伤/中毒会在回合末自扣 1/8 最大体力，
        //    污染"对手掉血"（坑集 §5）。
        constexpr int N = 20;
        long with_638 = 0, without_638 = 0;
        for (int i = 0; i < N; ++i) {
            with_638 += damage_trial(k638c, kBlind, /*apply_to=*/1);
            without_638 += damage_trial(k638c, kBlind, /*apply_to=*/-1);
        }
        const double r638 = static_cast<double>(with_638) / static_cast<double>(without_638);

        long with_758 = 0, without_758 = 0;
        for (int i = 0; i < N; ++i) {
            with_758 += damage_trial(k758, kConfusion, /*apply_to=*/1);
            without_758 += damage_trial(k758, kConfusion, /*apply_to=*/-1);
        }
        const double r758 = static_cast<double>(with_758) / static_cast<double>(without_758);
        std::cout << "  [T5840.c] 638 ratio=" << r638 << " 758 ratio=" << r758 << std::endl;
        CHECK(r638 > 1.3 && r638 < 1.7,
              "T5840.c 638 失明门伤害 ≈1.5×（实测 ratio=" + std::to_string(r638) + "）");
        CHECK(r758 > 1.7 && r758 < 2.3,
              "T5840.c 758 混乱门伤害 ≈2×（实测 ratio=" + std::to_string(r758) + "）");
    }

    std::cout << "scenario_5840 PASS (J253 条件威力强化族)" << std::endl;
    return 0;
}
